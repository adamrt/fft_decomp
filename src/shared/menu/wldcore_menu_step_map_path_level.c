#include "fft/script.h"
#include "fft/wldcore.h"

/*
 * Advances the world slideshow one frame: restarts it at the last page when
 * the level asks, steps the page timer forwards or backwards while the run
 * flag is set, and otherwise pops the menu level and resumes its parent.
 */
void wldcore_menu_step_map_path_level(wldcore_map_path_level_t* level) {
    s32 depth;
    s32 window_index;

    if (level->pending_sound != 0) {
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
            return;
        }
        if (level->erase != 0) {
            script_set_variable(g_wldcore_map_path_animation.script_variable + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 0);
            g_wldcore_map_path_animation.page_index = g_wldcore_map_path_animation.page_count - 1;
            g_wldcore_map_path_animation.frame_timer
                = g_wldcore_map_path_animation.page_frames[g_wldcore_map_path_animation.page_count - 1] - 1;
            wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_PATH);
        } else {
            wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_PATH);
        }
        level->pending_sound = 0;
    }
    if (g_wldcore_map_path_animation.flags & WLDCORE_SLIDESHOW_RUNNING) {
        if (level->erase == 0) {
            g_wldcore_map_path_animation.frame_timer++;
            if (g_wldcore_map_path_animation.frame_timer
                >= g_wldcore_map_path_animation.page_frames[g_wldcore_map_path_animation.page_index]) {
                g_wldcore_map_path_animation.page_index++;
                g_wldcore_map_path_animation.frame_timer = 0;
                if (g_wldcore_map_path_animation.page_index >= g_wldcore_map_path_animation.page_count) {
                    script_set_variable(
                        g_wldcore_map_path_animation.script_variable + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 1);
                }
            }
        } else {
            g_wldcore_map_path_animation.frame_timer--;
            if (g_wldcore_map_path_animation.frame_timer <= 0) {
                g_wldcore_map_path_animation.page_index--;
                if (g_wldcore_map_path_animation.page_index >= 0) {
                    g_wldcore_map_path_animation.frame_timer
                        = g_wldcore_map_path_animation.page_frames[g_wldcore_map_path_animation.page_index];
                }
            }
        }
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_NONE);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_depth = depth - 1;
    window_index = g_wldcore_menu_stack_records_next[depth - 2].window_index;
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    g_wldcore_window_records[window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    wldcore_menu_dispatch_resume_handler();
}
