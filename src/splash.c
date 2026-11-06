#include <stdint.h>
#include <stdlib.h>

#include <zos_sys.h>
#include <zvb_gfx.h>
#include <zgdk.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "audio.h"
#include "input.h"
#include "splash.h"

/*
 * Splash flow combines:
 * - TMX-based background rendering (like microbe)
 * - blocking press/release gate with blink cadence (like videopoker)
 */
static const uint8_t kSplashBlinkFrames = 24;
enum {
    SPLASH_TITLE_Y = 23,
    SPLASH_PROMPT_Y = SPLASH_TITLE_Y + 2
};
static const char kSplashPromptText[] = "PRESS ENTER TO PLAY!";
static const char kSplashPromptBlank[] = "                    ";

static void render_splash_map(void)
{
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        for (uint8_t x = 0; x < SCREEN_TILE_W; x++) {
            gfx_tilemap_place(&vctx, assets_get_splash_tile(x, y), TILEMAP_LAYER, x, y);
            gfx_tilemap_place(&vctx, assets_get_splash_overlay_tile(x, y), UI_LAYER, x, y);
        }
    }
}

static void draw_splash_prompt(uint8_t visible)
{
    const uint8_t len = (uint8_t)(sizeof(kSplashPromptText) - 1U);
    const uint8_t x = (uint8_t)((SCREEN_TILE_W - len) / 2U);

    if (visible) {
        hud_tilemap_nput_s(&vctx, kSplashPromptText, len, x, SPLASH_PROMPT_Y);
        return;
    }

    hud_tilemap_nput_s(&vctx, kSplashPromptBlank, len, x, SPLASH_PROMPT_Y);
}

void splash_run_blocking(void)
{
    uint8_t blink_counter = 0;
    uint8_t prompt_visible = 1;
    uint16_t input_state;
    uint16_t start_mask = (uint16_t)(BUTTON_START | BUTTON_B);

    input_flush_all();

    start_splash_music();
    render_splash_map();
    draw_splash_prompt(1);

    while (1) {
        tick_current_music();
        input_state = input_read_current();

        if (input_state & start_mask) {
            goto splash_pressed;
        }

        if (input_state & BUTTON_SELECT) {
            deinit();
            exit(0);
        }

        gfx_wait_vblank(&vctx);
        gfx_wait_end_vblank(&vctx);

        blink_counter++;
        if (blink_counter >= kSplashBlinkFrames) {
            blink_counter = 0;
            prompt_visible = (uint8_t)!prompt_visible;
            draw_splash_prompt(prompt_visible);
        }
    }

splash_pressed:
    stop_current_music();
    msleep(40);

    while (1) {
        input_state = input_read_current();

        if ((input_state & start_mask) == 0U) {
            break;
        }

        gfx_wait_vblank(&vctx);
        gfx_wait_end_vblank(&vctx);
    }

    input_flush_all();
}
