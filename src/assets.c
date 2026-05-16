#include <stdint.h>
#include <stddef.h>

#include <zos_vfs.h>
#include <zvb_gfx.h>
#include <zgdk/sound/tracker.h>

#include "main.h"
#include "assets.h"

/*
 * TMX layers exported via tiled2zeal:
 * - solitaire0000/0001 = gameplay board base/overlay
 * - splash0000/0001    = splash screen base/overlay
 */
extern uint8_t _ztm_solitaire0000_start;
extern uint8_t _ztm_solitaire0000_end;
extern uint8_t _ztm_solitaire0001_start;
extern uint8_t _ztm_solitaire0001_end;
extern uint8_t _ztm_splash0000_start;
extern uint8_t _ztm_splash0000_end;
extern uint8_t _ztm_splash0001_start;
extern uint8_t _ztm_splash0001_end;
extern uint8_t _zmt_splash_start;
extern uint8_t _zmt_splash_end;
extern uint8_t _zmt_music_start;
extern uint8_t _zmt_music_end;

#define CARD_TILE_W 3
#define CARD_TILE_H 4
#define CARD_TILE_COUNT (CARD_TILE_W * CARD_TILE_H)
#define CARD_RANK_COUNT 9
#define CARD_SUIT_COUNT 4

enum {
    COLOR_RED = 0,
    COLOR_BLACK = 1,
};

enum {
    RANK_ACE = 0,
    RANK_JACK = 6,
    RANK_QUEEN = 7,
    RANK_KING = 8,
};

typedef struct {
    uint8_t top;
    uint8_t mid;
    uint8_t bottom;
} FaceColumn;

typedef struct {
    uint8_t top;
    uint8_t mid_left;
    uint8_t mid_center;
    uint8_t mid_right;
    uint8_t bottom_left;
    uint8_t bottom_center;
    uint8_t bottom_right;
} QueenFace;

typedef enum {
    TOP_LEFT = 0,
    TOP_CENTRE,
    TOP_RIGHT,
    MIDDLE1_LEFT,
    MIDDLE1_CENTRE,
    MIDDLE1_RIGHT,
    MIDDLE2_LEFT,
    MIDDLE2_CENTRE,
    MIDDLE2_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTRE,
    BOTTOM_RIGHT,
} CardPos;

/* Helper macro used to encode pip positions for numeric ranks. */
#define PIPS(r0, r1, r2, r3) \
    ((uint16_t)((r0) | ((r1) << 3) | ((r2) << 6) | ((r3) << 9)))

static const uint8_t kWhiteCardTile = 16;

static const uint8_t kSuitTileBySuit[CARD_SUIT_COUNT] = {
    3,
    5,
    6,
    8
};

static const uint8_t kSuitColorBySuit[CARD_SUIT_COUNT] = {
    COLOR_RED,
    COLOR_RED,
    COLOR_BLACK,
    COLOR_BLACK,
};

/* A..6, J, Q, K */
static const uint8_t kRankGlyphRed[CARD_RANK_COUNT] = {
    48, 49, 50, 51, 52, 53, 58, 59, 60
};

/* A..6, J, Q, K */
static const uint8_t kRankGlyphBlack[CARD_RANK_COUNT] = {
    64, 65, 66, 67, 68, 69, 74, 75, 76
};

static const FaceColumn kJackFaceByColor[2] = {
    {1, 17, 33},
    {2, 18, 34},
};

static const FaceColumn kKingFaceByColor[2] = {
    {9, 25, 41},
    {10, 26, 42},
};

static const QueenFace kQueenFaceByColor[2] = {
    {4, 19, 20, 21, 35, 36, 37},
    {7, 22, 23, 24, 38, 39, 40},
};

/* 3x4 red back tile arrangement used for all hidden cards. */
static const uint8_t kBackCardTiles[CARD_TILE_H][CARD_TILE_W] = {
    {13, 14, 15},
    {29, 30, 31},
    {45, 46, 47},
    {61, 62, 63},
};

static const uint8_t kRemovedBackgroundTile = REMOVED_CARD_BACKGROUND_TILE_ID;

/* Stream a palette file into VRAM palette memory in chunks. */
static gfx_error load_palette_from_file(gfx_context* ctx, const char* path)
{
    uint8_t local_buf[512];
    uint8_t from_color = 0;
    zos_dev_t dev = open(path, O_RDONLY);

    if (dev < 0) {
        return GFX_FAILURE;
    }

    while (1) {
        uint16_t size = sizeof(local_buf);

        if (read(dev, local_buf, &size) != ERR_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        if (size == 0U) {
            break;
        }

        if (gfx_palette_load(ctx, local_buf, size, from_color) != GFX_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        from_color = (uint8_t)(from_color + (size / 2U));
    }

    close(dev);
    return GFX_SUCCESS;
}

/* Stream a tileset file into VRAM tile memory in chunks. */
static gfx_error load_tileset_from_file(gfx_context* ctx, const char* path, uint8_t compression_mode, uint16_t vram_scale)
{
    uint8_t local_buf[512];
    uint16_t from_byte = 0;
    zos_dev_t dev = open(path, O_RDONLY);

    if (dev < 0) {
        return GFX_FAILURE;
    }

    while (1) {
        uint16_t size = sizeof(local_buf);
        gfx_tileset_options options = {
            .compression = compression_mode,
            .from_byte = from_byte,
            .pal_offset = 0,
            .opacity = 0,
        };

        if (read(dev, local_buf, &size) != ERR_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        if (size == 0U) {
            break;
        }

        if (gfx_tileset_load(ctx, local_buf, size, &options) != GFX_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        from_byte = (uint16_t)(from_byte + (size * vram_scale));
    }

    close(dev);
    return GFX_SUCCESS;
}

/* Fill the full 3x4 card grid with one tile ID. */
static void init_card_grid(uint8_t grid[CARD_TILE_H][CARD_TILE_W], uint8_t tile)
{
    for (uint8_t row = 0; row < CARD_TILE_H; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            grid[row][col] = tile;
        }
    }
}

/* Address one logical card position and assign a tile. */
static void set_card_pos(uint8_t grid[CARD_TILE_H][CARD_TILE_W], CardPos pos, uint8_t tile)
{
    uint8_t row = (uint8_t)pos / CARD_TILE_W;
    uint8_t col = (uint8_t)pos % CARD_TILE_W;
    grid[row][col] = tile;
}

/* Place suit pips for numeric ranks A..6 using compact bitmasks. */
static void set_pips_for_rank(uint8_t grid[CARD_TILE_H][CARD_TILE_W], uint8_t rank, uint8_t suit_tile)
{
    static const uint16_t kPipMaskByRank[5] = {
        /* 2 */ PIPS(0b000, 0b010, 0b000, 0b010),
        /* 3 */ PIPS(0b000, 0b010, 0b010, 0b010),
        /* 4 */ PIPS(0b000, 0b101, 0b000, 0b101),
        /* 5 */ PIPS(0b000, 0b101, 0b010, 0b101),
        /* 6 */ PIPS(0b000, 0b101, 0b101, 0b101),
    };

    if (rank == RANK_ACE) {
        set_card_pos(grid, MIDDLE2_CENTRE, suit_tile);
        return;
    }

    if (rank < 1U || rank > 5U) {
        return;
    }

    {
        uint16_t mask = kPipMaskByRank[rank - 1U];
        for (uint8_t pos = 0; pos < CARD_TILE_COUNT; pos++) {
            if (mask & (uint16_t)(1U << pos)) {
                uint8_t row = (uint8_t)(pos / CARD_TILE_W);
                uint8_t col = (uint8_t)(pos % CARD_TILE_W);
                grid[row][col] = suit_tile;
            }
        }
    }
}

/* Place figure artwork for J/Q/K, choosing red/black palette variants. */
static void set_face_figure(uint8_t grid[CARD_TILE_H][CARD_TILE_W], uint8_t rank, uint8_t black, uint8_t suit_tile)
{
    uint8_t color_idx = black ? COLOR_BLACK : COLOR_RED;

    set_card_pos(grid, MIDDLE1_LEFT, suit_tile);

    if (rank == RANK_JACK) {
        const FaceColumn* face = &kJackFaceByColor[color_idx];
        set_card_pos(grid, MIDDLE1_CENTRE, face->top);
        set_card_pos(grid, MIDDLE2_CENTRE, face->mid);
        set_card_pos(grid, BOTTOM_CENTRE, face->bottom);
    } else if (rank == RANK_QUEEN) {
        const QueenFace* face = &kQueenFaceByColor[color_idx];
        set_card_pos(grid, MIDDLE1_CENTRE, face->top);
        set_card_pos(grid, MIDDLE2_LEFT, face->mid_left);
        set_card_pos(grid, MIDDLE2_CENTRE, face->mid_center);
        set_card_pos(grid, MIDDLE2_RIGHT, face->mid_right);
        set_card_pos(grid, BOTTOM_LEFT, face->bottom_left);
        set_card_pos(grid, BOTTOM_CENTRE, face->bottom_center);
        set_card_pos(grid, BOTTOM_RIGHT, face->bottom_right);
    } else if (rank == RANK_KING) {
        const FaceColumn* face = &kKingFaceByColor[color_idx];
        set_card_pos(grid, MIDDLE1_CENTRE, face->top);
        set_card_pos(grid, MIDDLE2_CENTRE, face->mid);
        set_card_pos(grid, BOTTOM_CENTRE, face->bottom);
    }
}

gfx_error load_cards_palette(gfx_context* ctx)
{
    return load_palette_from_file(ctx, "assets/cards.ztp");
}

gfx_error load_cards_tileset(gfx_context* ctx)
{
    if (load_tileset_from_file(ctx, "assets/cards.zts", TILESET_COMP_4BIT, 2) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    /*
     * Reserve tile 255 as a guaranteed clear/transparent tile so layer clears
     * never depend on artwork ordering inside cards.gif.
     */
    if (gfx_tileset_add_color_tile(ctx, EMPTY_TILE, 0) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    return GFX_SUCCESS;
}

/*
 * Convert compact card id -> 3x4 tile grid.
 * Card encoding is suit-major with 9 ranks per suit:
 *   rank = card % 9, suit = card / 9.
 */
void assets_build_card_tile_grid(uint8_t grid[4][3], uint8_t card)
{
    uint8_t rank = (uint8_t)(card % CARD_RANK_COUNT);
    uint8_t suit = (uint8_t)((card / CARD_RANK_COUNT) % CARD_SUIT_COUNT);
    uint8_t black = (kSuitColorBySuit[suit] == COLOR_BLACK);

    init_card_grid(grid, kWhiteCardTile);
    set_card_pos(grid, TOP_LEFT, black ? kRankGlyphBlack[rank] : kRankGlyphRed[rank]);

    if (rank < RANK_JACK) {
        set_pips_for_rank(grid, rank, kSuitTileBySuit[suit]);
    } else {
        set_face_figure(grid, rank, black, kSuitTileBySuit[suit]);
    }
}

/* Build red hidden-card art. */
void assets_build_back_tile_grid(uint8_t grid[4][3])
{
    for (uint8_t row = 0; row < CARD_TILE_H; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            grid[row][col] = kBackCardTiles[row][col];
        }
    }
}

/* Build the green background used for removed slots. */
void assets_build_removed_tile_grid(uint8_t grid[4][3])
{
    for (uint8_t row = 0; row < CARD_TILE_H; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            grid[row][col] = kRemovedBackgroundTile;
        }
    }
}

/* Read one tile from static TMX base layer (layer 0). */
uint8_t assets_get_layout_tile(uint8_t x, uint8_t y)
{
    const uint8_t* map = (const uint8_t*)&_ztm_solitaire0000_start;
    if (x >= SCREEN_TILE_W || y >= SCREEN_TILE_H) {
        return EMPTY_TILE;
    }
    return map[((uint16_t)y * SCREEN_TILE_W) + x];
}

/* Read one tile from static TMX overlay layer (layer 1). */
uint8_t assets_get_layout_overlay_tile(uint8_t x, uint8_t y)
{
    const uint8_t* map = (const uint8_t*)&_ztm_solitaire0001_start;
    if (x >= SCREEN_TILE_W || y >= SCREEN_TILE_H) {
        return EMPTY_TILE;
    }
    return map[((uint16_t)y * SCREEN_TILE_W) + x];
}

/* Read one tile from splash TMX base layer (layer 0). */
uint8_t assets_get_splash_tile(uint8_t x, uint8_t y)
{
    const uint8_t* map = (const uint8_t*)&_ztm_splash0000_start;
    if (x >= SCREEN_TILE_W || y >= SCREEN_TILE_H) {
        return EMPTY_TILE;
    }
    return map[((uint16_t)y * SCREEN_TILE_W) + x];
}

/* Read one tile from splash TMX overlay layer (layer 1). */
uint8_t assets_get_splash_overlay_tile(uint8_t x, uint8_t y)
{
    const uint8_t* map = (const uint8_t*)&_ztm_splash0001_start;
    if (x >= SCREEN_TILE_W || y >= SCREEN_TILE_H) {
        return EMPTY_TILE;
    }
    return map[((uint16_t)y * SCREEN_TILE_W) + x];
}

zos_err_t load_zmt(track_t* track, uint8_t index)
{
    zos_err_t err = ERR_INVALID_OFFSET;

    track->title[0] = (char)ZMT_INDEX_NONE;
    zmt_reset(VOL_50);

    switch (index) {
        case ZMT_INDEX_SPLASH: {
            const size_t size = (size_t)(&_zmt_splash_end - &_zmt_splash_start);
            err = zmt_rom_load(track, &_zmt_splash_start, size);
        } break;
        case ZMT_INDEX_GAME: {
            const size_t size = (size_t)(&_zmt_music_end - &_zmt_music_start);
            err = zmt_rom_load(track, &_zmt_music_start, size);
        } break;
        default:
            break;
    }

    if (err == ERR_SUCCESS) {
        track->title[0] = (char)index;
    }
    return err;
}

void __assets__(void) __naked
{
    /* Embed gameplay and splash TMX binaries directly in ROM. */
    __asm__(
        "__ztm_solitaire0000_start:\n"
        "    .incbin \"assets/solitaire0000.ztm\"\n"
        "__ztm_solitaire0000_end:\n"
        "__ztm_solitaire0001_start:\n"
        "    .incbin \"assets/solitaire0001.ztm\"\n"
        "__ztm_solitaire0001_end:\n"
        "__ztm_splash0000_start:\n"
        "    .incbin \"assets/splash0000.ztm\"\n"
        "__ztm_splash0000_end:\n"
        "__ztm_splash0001_start:\n"
        "    .incbin \"assets/splash0001.ztm\"\n"
        "__ztm_splash0001_end:\n"
        "__zmt_splash_start:\n"
        "    .incbin \"assets/splash.zmt\"\n"
        "__zmt_splash_end:\n"
        "__zmt_music_start:\n"
        "    .incbin \"assets/music.zmt\"\n"
        "__zmt_music_end:\n");
}
