#pragma once

#include <stdint.h>

/* Initialize/deinitialize audio + tracker runtime. */
void audio_init(void);
void audio_deinit(void);

/* Toggle pause/resume for gameplay music only. */
void audio_toggle_game_audio_mode(void);

/* Music lifecycle hooks. */
void start_splash_music(void);
void start_game_music(void);
void tick_current_music(void);
void stop_current_music(void);

/* One-shot SFX hook (not used yet in solitaire gameplay). */
void play_card_place_sound(void);
