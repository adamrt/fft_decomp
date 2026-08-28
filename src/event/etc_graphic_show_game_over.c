#include "fft/etc.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "psx/types.h"

void etc_graphic_show_game_over(s32 graphic_id) {
    /* The target reserves two otherwise unreferenced local words. */
    volatile s32 reserved_stack_words[2];
    s32 frame;
    s32 fade;
    s32 color;
    etc_graphic_dimensions_t* dimensions;

    g_battle_thread_call_target = main_sound_unload_scenario_music_and_tunes;
    battle_thread_call_on_main_stack();
    g_battle_thread_call_target = (void (*)(void))main_sound_open_scenario_smd_files;
    battle_thread_call_on_main_stack(0x32);
    do {
        battle_thread_yield();
        g_battle_thread_call_target = (void (*)(void))main_sound_poll_scenario_smd_load;
    } while (battle_thread_call_on_main_stack() != 0);
    g_battle_thread_call_target = (void (*)(void))main_sound_switch_music_track;
    battle_thread_call_on_main_stack(1, 0x7f, 0);

    dimensions = g_etc_graphics[graphic_id].dimensions;
    frame = 0;
    fade = 0;
    if (dimensions[2].height.signed_value > 0) {
        do {
            etc_graphic_build_game_over_polygons(
                graphic_id, fade, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
            frame++;
            battle_thread_yield();
            fade += g_battle_event_speed;
        } while (fade < dimensions[2].height.signed_value);
    }

    fade = 0;
    do {
        etc_graphic_build_game_over_polygons(
            graphic_id, dimensions[2].height.signed_value, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
        frame++;
        battle_thread_yield();
        fade += g_battle_event_speed;
    } while (fade < 0x258);

    color = 0x80;
    fade = 0;
    do {
        etc_graphic_build_game_over_polygons(
            graphic_id, dimensions[2].height.signed_value, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], color);
        color -= g_battle_event_speed;
        battle_thread_yield();
        fade += g_battle_event_speed;
        frame++;
    } while (fade < 0x80);
}
