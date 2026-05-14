#ifndef ZGOLF_COURSE_H
#define ZGOLF_COURSE_H

#include <zvb_gfx.h>

typedef struct {
    uint8_t ball_x;
    uint8_t ball_y;
    uint8_t hole_x;
    uint8_t hole_y;
    uint8_t par;
} course_info_t;

void course_load(gfx_context* ctx, uint8_t index);
uint8_t course_get_index(void);
uint8_t course_get_count(void);
uint8_t course_get_tile(uint8_t x, uint8_t y);
const course_info_t* course_get_info(void);
void course_render(gfx_context* ctx);

#endif
