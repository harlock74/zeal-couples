#pragma once

#include <stdint.h>
#include <zos_errors.h>

/*
 * One-shot input events consumed by gameplay.
 * Values are edge-triggered (pressed this frame only).
 */
typedef struct {
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
    uint8_t enter;
    uint8_t space;
    uint8_t toggle_music;
    uint8_t quit;
    uint8_t mouse_present;
    int8_t mouse_dx;
    int8_t mouse_dy;
    uint8_t mouse_left;
    uint8_t mouse_right;
} KeyEvents;

/* Initialize ZGDK input and prime internal edge-tracking state. */
zos_err_t input_events_init(void);

/* Poll one-shot events for this frame. */
void input_poll_events(KeyEvents* ev);

/* Read current held button mask (keyboard + sanitized controller). */
uint16_t input_read_current(void);

/* Reset edge-tracker to current held state. */
void input_reset_edges(void);

/* Flush keyboard/controller FIFOs and reset edge-tracker. */
void input_flush_all(void);
