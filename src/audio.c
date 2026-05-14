#include <stdint.h>

#include <zvb_sound.h>
#include <zgdk.h>
#include <zgdk/sound/tracker.h>

#include "audio.h"
#include "assets.h"

/*
 * Tracker storage owned by this module.
 * We keep a single track object and swap ROM song data by index.
 */
static pattern_t music_pattern0;
static pattern_t music_pattern1;
static pattern_t music_pattern2;
static pattern_t music_pattern3;
static pattern_t music_pattern4;
static pattern_t music_pattern5;
static pattern_t music_pattern6;
static pattern_t music_pattern7;
static track_t music_track = {
    .title = {(char)ZMT_INDEX_NONE, '\0'},
    .patterns = {
        &music_pattern0,
        &music_pattern1,
        &music_pattern2,
        &music_pattern3,
        &music_pattern4,
        &music_pattern5,
        &music_pattern6,
        &music_pattern7,
    }
};

/* Runtime music flags:
 * - music_playing: current tracker tick should advance
 * - game_music_paused: user toggle for gameplay track only (P key)
 */
static uint8_t music_playing = 0;
static uint8_t game_music_paused = 0;

void audio_init(void)
{
    sound_init();
    zmt_reset(VOL_50);
    music_playing = 0;
}

void audio_deinit(void)
{
    zmt_sound_off();
    sound_stop_all();
    sound_deinit();
    music_playing = 0;
}

void start_splash_music(void)
{
    /* Stop any currently sounding tracker channels before song switch. */
    zmt_sound_off();

    if ((uint8_t)music_track.title[0] != ZMT_INDEX_SPLASH) {
        if (load_zmt(&music_track, ZMT_INDEX_SPLASH) != ERR_SUCCESS) {
            music_playing = 0;
            return;
        }
    }

    zmt_track_reset(&music_track, 1);
    music_playing = 1;
}

void start_game_music(void)
{
    /* Stop currently sounding channels; track may be reset/restarted below. */
    zmt_sound_off();

    if (game_music_paused) {
        music_playing = 0;
        return;
    }

    if ((uint8_t)music_track.title[0] != ZMT_INDEX_GAME) {
        if (load_zmt(&music_track, ZMT_INDEX_GAME) != ERR_SUCCESS) {
            music_playing = 0;
            return;
        }
    }

    zmt_track_reset(&music_track, 1);
    music_playing = 1;
}

void tick_current_music(void)
{
    /* Keep non-tracker audio runtime serviced even if music is paused/stopped. */
    sound_loop();

    if (music_playing) {
        zmt_tick(&music_track, 1);
    }
}

void stop_current_music(void)
{
    zmt_sound_off();
    music_playing = 0;
}

void audio_toggle_game_audio_mode(void)
{
    game_music_paused ^= 1U;

    if (game_music_paused) {
        /*
         * Pause request:
         * - silence channels now
         * - keep track data loaded to allow resume without reload
         */
        if (music_playing && (uint8_t)music_track.title[0] == ZMT_INDEX_GAME) {
            zmt_sound_off();
            music_playing = 0;
        }
        return;
    }

    /*
     * Resume request:
     * - if gameplay track already loaded, continue ticking from current state
     * - otherwise start gameplay track from beginning
     */
    if ((uint8_t)music_track.title[0] == ZMT_INDEX_GAME) {
        music_playing = 1;
    } else {
        start_game_music();
    }
}

void play_card_place_sound(void)
{
    /* Reserved hook for future card SFX. */
}
