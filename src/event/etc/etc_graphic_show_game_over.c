#include "fft/event_etc.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Show the game-over graphic: swap the scenario music for the game-over
 * track, reveal the graphic from the left, one pixel column (one row of the
 * rotated texture) per event-speed step, hold it for 600 steps, then fade its
 * brightness from 0x80 to 0 over 128 steps. The two primitive buffers
 * alternate by frame.
 */
void etc_graphic_show_game_over(s32 graphic_id) {
    /* The target reserves two otherwise unreferenced local 32-bit values. */
    s32 reserved_stack_words[2];
    s32 frame;
    s32 fade;
    s32 color;
    sprite_sheet_frame_t* dimensions;

    g_thread_call_target = main_sound_unload_scenario_music_and_tunes;
    thread_call_on_main_stack();
    g_thread_call_target = (void (*)(void))main_sound_open_scenario_smd_files;
    thread_call_on_main_stack(MAIN_MUSIC_GAME_OVER);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_sound_poll_scenario_smd_load;
    } while (thread_call_on_main_stack() != 0);
    g_thread_call_target = (void (*)(void))main_sound_switch_music_track;
    thread_call_on_main_stack(1, 0x7f, 0);

    dimensions = g_etc_graphics[graphic_id].dimensions;
    frame = 0;
    fade = 0;
    if (dimensions[2].source.h > 0) {
        do {
            etc_graphic_build_game_over_polygons(
                graphic_id, fade, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
            frame++;
            thread_yield();
            fade += g_script_event_speed;
        } while (fade < dimensions[2].source.h);
    }

    fade = 0;
    do {
        etc_graphic_build_game_over_polygons(
            graphic_id, dimensions[2].source.h, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
        frame++;
        thread_yield();
        fade += g_script_event_speed;
    } while (fade < 0x258);

    color = 0x80;
    fade = 0;
    do {
        etc_graphic_build_game_over_polygons(
            graphic_id, dimensions[2].source.h, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], color);
        color -= g_script_event_speed;
        thread_yield();
        fade += g_script_event_speed;
        frame++;
    } while (fade < 0x80);
}
