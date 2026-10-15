#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Per-frame input update of the formation, shop and name-entry screens.
 *
 * Reads the pad (through the tutorial input reader), then clears the 32-bit input masks during steps 1..3 of the
 * window scale animation, while the text thread runs or a help screen is
 * open, on the frame the text ends and during a screen fade. The text phase
 * goes 1 on the first frame of a text task (which queues the window-open
 * sound), 2 after it and 0 otherwise. When the text thread is not running
 * the task-active flag and the held-loop sound release are cleared.
 */
void world_input_update_with_message_state(void) {
    s32 running = 0;
    s32 step;
    u8 value;

    world_script_update_tutorial_controller_input();
    step = world_menu_get_window_scale_step();
    if (step >= 1 && step <= 3) {
        menu_input_clear_state();
    } else {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        if (running != 0 || g_world_grid_menu_id != MENU_GRID_HELP_NONE) {
            menu_input_clear_state();
            if (running != 0 && g_menu_input_lock_mode != 0) {
                value = 1;
                if (g_world_text_task_phase != 0) {
                    value = 2;
                }
                g_world_text_task_phase = value;
            } else {
                g_world_text_task_phase = 0;
            }
        } else if (g_world_text_task_phase != 0 || world_gfx_get_fade_state() != WORLD_FADE_STATE_NONE) {
            g_world_text_task_phase = 0;
            menu_input_clear_state();
        }
    }
    if (running == 0) {
        g_menu_input_lock_mode = 0;
        g_world_sound_release_held_loop = 0;
    }
    if (g_world_text_task_phase == 1) {
        g_world_menu_sound_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
