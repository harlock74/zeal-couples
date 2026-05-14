#include <stdint.h>

#include <core.h>
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_video.h>
#include <zgdk/input.h>
#include <zvb_gfx.h>
#include <zgdk.h>
#include <zgdk/tilemap/scroll.h>

#include "assets.h"
#include "config.h"
#include "course.h"
#include "hud.h"
#include "main.h"
#include "objects.h"

gfx_context vctx;
static uint8_t input_ready = 0;
static uint16_t input_prev = 0;
static uint8_t aim_input_wait = 0;
static uint8_t aim_held_steps = 0;
static uint16_t aim_held_mask = 0;
static uint16_t aim_blocked_mask = 0;
static uint8_t charging_shot = 0;
static int16_t total_score = 0;

static void render_hud(void)
{
    const course_info_t* course = course_get_info();

    hud_render_values(&vctx,
        (uint8_t)(course_get_index() + 1),
        course->par,
        objects_get_strokes(),
        total_score);
}

static void load_course(uint8_t index)
{
    const course_info_t* course;

    course_load(&vctx, index);
    course = course_get_info();
    objects_reset_ball_to_tile(course->ball_x, course->ball_y);
    objects_reset_strokes();
    hud_render_static(&vctx);
    hud_render_power_bar(&vctx, 0, 0);
    hud_render_water_message(&vctx, 0);
    render_hud();
    charging_shot = 0;
}

static uint16_t clear_aim_input(uint16_t input)
{
    const uint16_t aim_mask = BUTTON_LEFT | BUTTON_RIGHT;

    input_prev &= (uint16_t)~aim_mask;
    aim_input_wait = 0;
    aim_held_steps = 0;
    aim_held_mask = 0;
    aim_blocked_mask = 0;

    return (uint16_t)(input & (uint16_t)~aim_mask);
}

static uint16_t input_read_controller(void)
{
    uint16_t controller_input = controller_read();
    uint16_t unused_bits = (uint16_t)(
        BUTTON_UNUSED1 |
        BUTTON_UNUSED2 |
        BUTTON_UNUSED3 |
        BUTTON_UNUSED4);
    uint16_t controller_buttons = (uint16_t)(
        BUTTON_B |
        BUTTON_Y |
        BUTTON_SELECT |
        BUTTON_START |
        BUTTON_UP |
        BUTTON_DOWN |
        BUTTON_LEFT |
        BUTTON_RIGHT |
        BUTTON_A |
        BUTTON_X |
        BUTTON_L |
        BUTTON_R);

    /*
     * Detached/noisy controller lines can look like impossible input states.
     * Ignore them so keyboard input is not hidden by floating SNES bits.
     */
    if (controller_input & unused_bits) {
        return 0;
    }

    if ((controller_input & controller_buttons) == controller_buttons) {
        return 0;
    }

    return (uint16_t)(controller_input & controller_buttons);
}

static uint16_t input_read_current(void)
{
    if (!input_ready) {
        return 0;
    }

    return (uint16_t)(keyboard_read() | input_read_controller());
}

static uint16_t aim_direction_mask(uint16_t input)
{
    const uint16_t aim_mask = BUTTON_LEFT | BUTTON_RIGHT;

    if ((input & aim_mask) == BUTTON_LEFT) {
        return BUTTON_LEFT;
    }

    if ((input & aim_mask) == BUTTON_RIGHT) {
        return BUTTON_RIGHT;
    }

    return 0;
}

static uint16_t filter_aim_input(uint16_t input)
{
    uint16_t direction = aim_direction_mask(input);

    if (direction == 0 || (input & SHOOT_BUTTON_MASK) || objects_ball_is_rolling()) {
        aim_held_steps = 0;
        aim_held_mask = 0;
        aim_blocked_mask = 0;
        return input;
    }

    if (direction != aim_held_mask) {
        aim_held_steps = 0;
        aim_held_mask = direction;
        aim_blocked_mask = 0;
    }

    return (uint16_t)(input & (uint16_t)~aim_blocked_mask);
}

static void note_aim_rotation(uint16_t direction)
{
    if (direction == 0) {
        return;
    }

    if (direction != aim_held_mask) {
        aim_held_steps = 0;
        aim_held_mask = direction;
        aim_blocked_mask = 0;
    }

    if (aim_held_steps < AIM_INPUT_MAX_HELD_STEPS) {
        aim_held_steps++;
    }

    if (aim_held_steps >= AIM_INPUT_MAX_HELD_STEPS) {
        aim_blocked_mask = direction;
        aim_input_wait = 0;
    }
}

int main(void)
{
    init();

    while (1) {
        uint16_t input = input_read_current();
        uint8_t rotate_left = 0;
        uint8_t rotate_right = 0;

        input = filter_aim_input(input);

        if ((input & EXIT_BUTTON_MASK) && !(input_prev & EXIT_BUTTON_MASK)) {
            deinit();
            exit(0);
        }

        if (aim_input_wait > 0) {
            aim_input_wait--;
        }

        if (!(input & (BUTTON_LEFT | BUTTON_RIGHT)) ||
            ((input & BUTTON_LEFT) && (input & BUTTON_RIGHT))) {
            aim_input_wait = 0;
        }

        if (!objects_ball_is_rolling() && !((input & BUTTON_LEFT) && (input & BUTTON_RIGHT))) {
            if ((input & BUTTON_LEFT) && !(input_prev & BUTTON_LEFT)) {
                rotate_left = 1;
            } else if ((input & BUTTON_LEFT) && !(input & BUTTON_RIGHT) && aim_input_wait == 0) {
                rotate_left = 1;
            }

            if ((input & BUTTON_RIGHT) && !(input_prev & BUTTON_RIGHT)) {
                rotate_right = 1;
            } else if ((input & BUTTON_RIGHT) && !(input & BUTTON_LEFT) && aim_input_wait == 0) {
                rotate_right = 1;
            }
        }

        if (rotate_left) {
            objects_aim_left();
            note_aim_rotation(BUTTON_LEFT);
            aim_input_wait = AIM_INPUT_REPEAT_FRAMES;
        }

        if (rotate_right) {
            objects_aim_right();
            note_aim_rotation(BUTTON_RIGHT);
            aim_input_wait = AIM_INPUT_REPEAT_FRAMES;
        }

        if (!objects_ball_is_rolling() &&
            (input & SHOOT_BUTTON_MASK) &&
            !(input_prev & SHOOT_BUTTON_MASK)) {
            charging_shot = 1;
            objects_begin_charge();
            input = clear_aim_input(input);
        }

        if ((input & SHOOT_BUTTON_MASK) && charging_shot) {
            objects_charge_shot();
        }

        if (!(input & SHOOT_BUTTON_MASK) && charging_shot) {
            charging_shot = 0;
            objects_release_shot();
            input = clear_aim_input(input);
        }

        objects_update_ball();
        if (objects_hole_is_complete()) {
            const course_info_t* course = course_get_info();
            uint8_t next_course = (uint8_t)(course_get_index() + 1);

            total_score = (int16_t)(total_score + ((int16_t)objects_get_strokes() - course->par));

            if (next_course >= course_get_count()) {
                next_course = 0;
                total_score = 0;
            }

            load_course(next_course);
        }

        input_prev = input;
        hud_render_power_bar(&vctx, charging_shot, objects_get_power_segments());
        hud_render_water_message(&vctx, objects_water_was_hit());
        render_hud();
        objects_render(&vctx);
    }
}

void handle_error(zos_err_t err, const char* message, uint8_t fatal)
{
    if (err != ERR_SUCCESS) {
        if (fatal) {
            deinit();
        }

        put_s("\nError[");
        put_u8((uint8_t)err);
        put_s("] ");
        put_s(message);

        if (fatal) {
            exit(err);
        }
    }
}

void init(void)
{
    zos_err_t err;
    gfx_tileset_options tileset_options = {
        .compression = TILESET_COMP_4BIT,
    };

    gfx_enable_screen(0);

    err = input_init(1);
    handle_error(err, "failed to init input", 1);
    input_ready = 1;
    input_flush();
    input_prev = input_read_current();

    err = gfx_initialize(SCREEN_MODE, &vctx);
    handle_error(err, "failed to init graphics", 1);

    err = assets_load_palette(&vctx);
    handle_error(err, "failed to load palette", 1);

    err = assets_load_tileset(&vctx, &tileset_options);
    handle_error(err, "failed to load tileset", 1);

    err = gfx_tileset_add_color_tile(&vctx, EMPTY_TILE, 0);
    handle_error(err, "failed to create empty tile", 1);

    hud_init();

    tilemap_fill(&vctx, COURSE_LAYER, EMPTY_TILE, 0, 0, SCREEN_TILES_W, SCREEN_TILES_H);
    tilemap_fill(&vctx, UI_LAYER, EMPTY_TILE, 0, 0, SCREEN_TILES_W, SCREEN_TILES_H);

    course_load(&vctx, 0);

    err = objects_init();
    handle_error(err, "failed to init objects", 1);

    hud_render_static(&vctx);
    render_hud();
    objects_render(&vctx);

    gfx_enable_screen(1);
}

void deinit(void)
{
    tilemap_scroll(COURSE_LAYER, 0, 0);
    tilemap_scroll(UI_LAYER, 0, 0);
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}
