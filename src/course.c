#include <stdint.h>
#include <zvb_gfx.h>

#include "assets.h"
#include "config.h"
#include "course.h"

static const course_info_t course_infos[GAME_COURSE_COUNT] = {
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
    { BALL_START_TILE_X, BALL_START_TILE_Y, COURSE_DEFAULT_HOLE_TILE_X, COURSE_DEFAULT_HOLE_TILE_Y, COURSE_DEFAULT_PAR },
};

void course_load(gfx_context* ctx, uint8_t index)
{
    assets_set_course_index(index);
    course_render(ctx);
}

uint8_t course_get_index(void)
{
    return assets_get_course_index();
}

uint8_t course_get_count(void)
{
    return assets_get_course_count();
}

const course_info_t* course_get_info(void)
{
    return &course_infos[course_get_index()];
}

uint8_t course_get_tile(uint8_t x, uint8_t y)
{
    const course_info_t* info = course_get_info();

    if (x == info->hole_x && y == info->hole_y) {
        return TILE_ID_HOLE;
    }

    return assets_get_course_tile(x, y);
}

void course_render(gfx_context* ctx)
{
    for (uint8_t y = 0; y < SCREEN_TILES_H; y++) {
        for (uint8_t x = 0; x < SCREEN_TILES_W; x++) {
            uint8_t tile = course_get_tile(x, y);

            gfx_tilemap_place(ctx, tile, COURSE_LAYER, x, y);
        }
    }
}
