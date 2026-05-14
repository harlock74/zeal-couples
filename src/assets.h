#ifndef ZGOLF_ASSETS_H
#define ZGOLF_ASSETS_H

#include <stdint.h>
#include <zvb_gfx.h>

void __assets__(void);

extern uint8_t _palette_start;
extern uint8_t _palette_end;
extern uint8_t _tileset_start;
extern uint8_t _tileset_end;

gfx_error assets_load_palette(gfx_context* ctx);
gfx_error assets_load_tileset(gfx_context* ctx, gfx_tileset_options* options);

uint8_t assets_get_course_count(void);
uint8_t assets_get_course_index(void);
void assets_set_course_index(uint8_t index);
uint8_t assets_get_course_tile(uint8_t x, uint8_t y);

#endif
