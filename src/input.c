#include <stdint.h>

#include <zgdk/input.h>

#include "input.h"

enum {
    EVENT_NOT_PRESSED = 0,
    EVENT_PRESSED = 1,
    INPUT_USE_CONTROLLER = 1,
    INPUT_MOUSE_PORT_NONE = 0xFF,
};

static uint8_t input_ready = 0;
static uint16_t previous_input = 0;
static uint16_t previous_mouse_buttons = 0;
static uint8_t mouse_port = INPUT_MOUSE_PORT_NONE;

static uint16_t read_controller_input(void)
{
    uint16_t controller_input = controller_read();
    uint16_t unused_bits = (uint16_t)(
        BUTTON_UNUSED1 |
        BUTTON_UNUSED2 |
        BUTTON_UNUSED3 |
        BUTTON_UNUSED4);
    uint16_t all_controller_buttons = (uint16_t)(
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
     * Learned from zeal-solitaire hardening:
     * detached/floating controller lines can report invalid states in some
     * emulator setups. Ignore obviously invalid controller reads so keyboard
     * edges are not masked by noisy gamepad bits.
     */
    if (controller_input & unused_bits) {
        return 0U;
    }

    if ((controller_input & all_controller_buttons) == all_controller_buttons) {
        return 0U;
    }

    return (uint16_t)(controller_input & all_controller_buttons);
}

static uint16_t read_mouse_buttons(void)
{
    if (mouse_port == INPUT_MOUSE_PORT_NONE) {
        return 0U;
    }

    return (uint16_t)(controller_get(mouse_port) & (MOUSE_LEFT | MOUSE_RIGHT));
}

uint16_t input_read_current(void)
{
    if (!input_ready) {
        return 0U;
    }

    /*
     * We still initialize via input_init(1) per ZGDK API, but read keyboard
     * and controller explicitly here to sanitize controller noise.
     */
    return (uint16_t)(keyboard_read() | read_controller_input());
}

void input_reset_edges(void)
{
    previous_input = input_read_current();
    previous_mouse_buttons = read_mouse_buttons();
}

void input_flush_all(void)
{
    input_flush();
    input_reset_edges();
}

zos_err_t input_events_init(void)
{
    zos_err_t err = input_init(INPUT_USE_CONTROLLER);
    if (err != ERR_SUCCESS) {
        input_ready = 0;
        return err;
    }

    input_ready = 1;
    mouse_port = INPUT_MOUSE_PORT_NONE;
    if (controller_is(SNES_PORT1, SNES_MOUSE)) {
        mouse_port = SNES_PORT1;
    } else if (controller_is(SNES_PORT2, SNES_MOUSE)) {
        mouse_port = SNES_PORT2;
    }

    if (mouse_port != INPUT_MOUSE_PORT_NONE) {
        controller_set_mouse_sensitivity(mouse_port, MOUSE_MEDIUM);
        controller_set_mouse_sensitivity(mouse_port, MOUSE_MEDIUM);
    }

    input_reset_edges();
    return ERR_SUCCESS;
}

void input_poll_events(KeyEvents* ev)
{
    uint16_t current_input;
    uint16_t pressed_input;
    uint16_t current_mouse_buttons;
    uint16_t pressed_mouse_buttons;
    uint8_t mouse_moved = 0;

    ev->up = EVENT_NOT_PRESSED;
    ev->down = EVENT_NOT_PRESSED;
    ev->left = EVENT_NOT_PRESSED;
    ev->right = EVENT_NOT_PRESSED;
    ev->enter = EVENT_NOT_PRESSED;
    ev->space = EVENT_NOT_PRESSED;
    ev->toggle_music = EVENT_NOT_PRESSED;
    ev->quit = EVENT_NOT_PRESSED;
    ev->mouse_present = EVENT_NOT_PRESSED;
    ev->mouse_dx = 0;
    ev->mouse_dy = 0;
    ev->mouse_left = EVENT_NOT_PRESSED;
    ev->mouse_right = EVENT_NOT_PRESSED;

    if (!input_ready) {
        return;
    }

    current_input = input_read_current();
    pressed_input = (uint16_t)(current_input & (uint16_t)~previous_input);
    if (mouse_port != INPUT_MOUSE_PORT_NONE) {
        mouse_moved = controller_read_mouse(mouse_port);
    }
    current_mouse_buttons = read_mouse_buttons();
    pressed_mouse_buttons = (uint16_t)(
        current_mouse_buttons & (uint16_t)~previous_mouse_buttons);

    ev->up = (pressed_input & BUTTON_UP) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->down = (pressed_input & BUTTON_DOWN) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->left = (pressed_input & BUTTON_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->right = (pressed_input & BUTTON_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->enter = (pressed_input & BUTTON_START) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->space = (pressed_input & BUTTON_B) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->toggle_music = (pressed_input & BUTTON_X) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->quit = (pressed_input & BUTTON_SELECT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_present = (mouse_port != INPUT_MOUSE_PORT_NONE) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_left = (pressed_mouse_buttons & MOUSE_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_right = (pressed_mouse_buttons & MOUSE_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;

    if (mouse_moved) {
        ev->mouse_dx = controller_get_mousex();
        ev->mouse_dy = controller_get_mousey();
    }

    previous_input = current_input;
    previous_mouse_buttons = current_mouse_buttons;
}
