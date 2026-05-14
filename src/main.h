#ifndef ZGOLF_MAIN_H
#define ZGOLF_MAIN_H

#include <zos_errors.h>
#include <zvb_gfx.h>

void handle_error(zos_err_t err, const char* message, uint8_t fatal);
void init(void);
void deinit(void);

extern gfx_context vctx;

#endif
