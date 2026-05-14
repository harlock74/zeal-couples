#ifndef ZGOLF_OBJECTS_H
#define ZGOLF_OBJECTS_H

#include <zos_errors.h>
#include <zvb_gfx.h>

zos_err_t objects_init(void);
void objects_reset_ball_to_tile(uint8_t tile_x, uint8_t tile_y);
void objects_aim_left(void);
void objects_aim_right(void);
void objects_begin_charge(void);
void objects_charge_shot(void);
void objects_release_shot(void);
uint8_t objects_get_power_segments(void);
uint8_t objects_water_was_hit(void);
void objects_update_ball(void);
void objects_update_aim_markers(void);
void objects_render(gfx_context* ctx);
uint8_t objects_ball_is_rolling(void);
uint8_t objects_hole_is_complete(void);
uint16_t objects_get_strokes(void);
void objects_reset_strokes(void);

#endif
