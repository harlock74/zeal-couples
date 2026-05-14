#include <stdint.h>
#include <zvb_gfx.h>

#include "assets.h"
#include "config.h"
#include "course_assets.generated.inc"

static uint8_t active_course_index = 0;

gfx_error assets_load_palette(gfx_context* ctx)
{
    const uint16_t palette_size = (uint16_t)(&_palette_end - &_palette_start);
    return gfx_palette_load(ctx, &_palette_start, palette_size, 0);
}

gfx_error assets_load_tileset(gfx_context* ctx, gfx_tileset_options* options)
{
    uint8_t* tileset = &_tileset_start;
    uint16_t remaining = (uint16_t)(&_tileset_end - &_tileset_start);
    uint16_t source_offset = 0;
    uint16_t vram_offset = 0;

    while (remaining > 0) {
        uint16_t chunk_size = remaining;
        gfx_error err;
        gfx_tileset_options chunk_options = {
            .compression = options->compression,
            .from_byte = vram_offset,
            .pal_offset = options->pal_offset,
            .opacity = options->opacity,
        };

        if (chunk_size > TILESET_LOAD_CHUNK_BYTES) {
            chunk_size = TILESET_LOAD_CHUNK_BYTES;
        }

        err = gfx_tileset_load(ctx, tileset + source_offset, chunk_size, &chunk_options);
        if (err != GFX_SUCCESS) {
            return err;
        }

        remaining = (uint16_t)(remaining - chunk_size);
        source_offset = (uint16_t)(source_offset + chunk_size);
        vram_offset = (uint16_t)(vram_offset + (chunk_size * TILESET_4BIT_VRAM_SCALE));
    }

    return GFX_SUCCESS;
}

uint8_t assets_get_course_tile(uint8_t x, uint8_t y)
{
    const uint8_t* map = (const uint8_t*)course_maps[active_course_index];

    if (x >= COURSE_TILES_W || y >= COURSE_TILES_H) {
        return EMPTY_TILE;
    }

    return (uint8_t)(map[((uint16_t)y * COURSE_TILES_W) + x] + TILEMAP_GID_OFFSET);
}

uint8_t assets_get_course_count(void)
{
    if (course_map_count > GAME_COURSE_COUNT) {
        return GAME_COURSE_COUNT;
    }

    return course_map_count;
}

uint8_t assets_get_course_index(void)
{
    return active_course_index;
}

void assets_set_course_index(uint8_t index)
{
    if (index >= assets_get_course_count()) {
        index = 0;
    }

    active_course_index = index;
}

void __assets__(void) __naked
{
    __asm__(
        "__palette_start:\n"
        "    .incbin \"assets/minigolf.ztp\"\n"
        "__palette_end:\n"

        "__tileset_start:\n"
        "    .incbin \"assets/minigolf.zts\"\n"
        "__tileset_end:\n"
        COURSE_MAPS_ASM
    );
}
