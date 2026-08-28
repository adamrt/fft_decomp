#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Second link name for g_main_system_flags (0x8004d950), as in
 * wldcore_list_handle_tutorial_mask_1_entries_input.c. */
extern s32 g_main_system_flags_alias;

s32 wldcore_script_run_until_yield(void);
void wldcore_capture_state_and_play_time(void);
void wldcore_menu_push_sound_novel_quit_level(void);
void wldcore_menu_push_sound_novel_history_level(void);
void wldcore_menu_push_screen_transition_level(s32 screen, s32 param_a, s32 param_b);
void world_thread_set_task_id_to_three(s32 thread_id);

/*
 * Per-frame handler for a WLDCORE brightness/fade menu level.
 *
 * The system-flag test reads g_main_system_flags_alias, a second link name for
 * g_main_system_flags: the target re-loads the word for the `^= 4` in phase 2
 * instead of reusing the tested value, as in wldcore_menu_step_sound_novel_resume_level. The unused array
 * reproduces the target's 0x40-byte frame. The 0x140 state-flag test is a
 * word load (`lw`) of the u16 g_wldcore_active_saved_record.state_flags in the target, so it
 * reads the flags through an s32 cast; the u16 declaration gives `lhu`.
 */
void wldcore_menu_step_sound_novel_level(wldcore_menu_sound_novel_level_t* level) {
    s32 unused[2];
    u16* state_flags;
    wldcore_sortbox_state_t* fade;
    s16* brightness;
    u32 buttons;
    s32 width;
    s32 window;
    u16 shade;

    if ((g_main_system_flags_alias & 0xC) == 4) {
        switch (level->phase) {
        case 2:
            if (level->countdown == 0) {
                g_main_system_flags ^= 4;
                wldcore_menu_push_sound_novel_history_level();
                return;
            }
            level->countdown--;
            return;
        case 3:
            if (world_thread_is_running(0xE) != 0) {
                return;
            }
            g_wldcore_menu_result = 0;
            g_main_system_flags ^= 4;
            wldcore_capture_state_and_play_time();
            g_wldcore_screen_fade_box_0_priority = 0xF;
            g_wldcore_screen_fade_box_1_flags = 0;
            g_wldcore_window_record_count -= 3;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 5;
            g_wldcore_menu_stack_depth--;
            fade = &g_wldcore_screen_fade_state;
            fade->flags[0] = (fade->flags[0] & ~8) | 2;
            main_sound_stop_weather_sfx_music();
            main_sound_remove_vfx_resource(g_wldcore_sound_novel_sound_resource);
            /* The target loads g_wldcore_saved_record_index into a0 for this argument-less callee. */
            ((void (*)(s32))wldcore_reset_selected_saved_record)(g_wldcore_saved_record_index);
            g_wldcore_sound_novel_countdown_frames = 0;
            wldcore_menu_push_screen_transition_level(0x1B, level->message, 0);
            return;
        }
    }

    state_flags = &g_wldcore_active_saved_record.state_flags;
    if (*state_flags & 0x20) {
        return;
    }

    if (wldcore_script_run_until_yield() != 0) {
        if (g_wldcore_current_button_input & (PSX_PAD_L1 | PSX_PAD_R1)) {
            if (g_wldcore_current_button_input & PSX_PAD_L1) {
                if (g_wldcore_active_saved_record.brightness < 0xFF) {
                    g_wldcore_active_saved_record.brightness++;
                }
            }
            if (g_wldcore_current_button_input & PSX_PAD_R1) {
                brightness = &g_wldcore_active_saved_record.brightness;
                if (*brightness != 0) {
                    (*brightness)--;
                }
            }
            shade = g_wldcore_active_saved_record.brightness;
            g_wldcore_screen_fade_state.boxes[1].r = shade;
            g_wldcore_screen_fade_state.boxes[1].g = shade;
            g_wldcore_screen_fade_state.boxes[1].b = shade;
            level->fade_timer = 0x3C;
        }

        if (level->fade_timer != 0) {
            g_wldcore_window_render_records[level->render_index].flags &= ~0x10;
            g_wldcore_window_records[level->overlay_window].flags &= ~0x10;
            width = 0x100 - g_wldcore_active_saved_record.brightness;
            g_wldcore_window_render_records[level->render_index].width = width / 4;
            level->fade_timer--;
        } else {
            g_wldcore_window_render_records[level->render_index].flags |= 0x10;
            g_wldcore_window_records[level->overlay_window].flags |= 0x10;
        }

        if (world_menu_get_event_state_flag() != 0) {
            buttons = g_wldcore_new_button_presses;
            if (buttons & PSX_PAD_CIRCLE) {
                return;
            }
            g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
                = 0;
            if (buttons & PSX_PAD_SELECT) {
                wldcore_menu_push_message_level(0x1036, 1);
                return;
            }
            if ((buttons & PSX_PAD_START) && (*(s32*)&g_wldcore_active_saved_record.state_flags & 0x140) == 0x40) {
                g_wldcore_window_records[level->left_window].sequence = 0x6F;
                g_wldcore_window_records[level->left_window].frame_index
                    = g_wldcore_window_records[level->left_window].anim_counter = 0;
                level->fade_timer = 0;
                g_wldcore_window_render_records[level->render_index].flags |= 0x10;
                g_wldcore_window_records[level->overlay_window].flags |= 0x10;
                wldcore_menu_push_sound_novel_quit_level();
                return;
            }
            if ((g_wldcore_current_button_input & PSX_PAD_SQUARE) && (g_wldcore_active_saved_record.state_flags & 0x40)
                && (s16)g_wldcore_active_saved_record.text_history_3 != -1) {
                g_wldcore_window_records[level->right_window].sequence = 0x71;
                window = level->right_window;
                g_wldcore_window_records[window].anim_counter = 0;
                g_wldcore_window_records[window].frame_index = 0;
                level->fade_timer = 0;
                g_wldcore_window_render_records[level->render_index].flags |= 0x10;
                g_wldcore_window_records[level->overlay_window].flags |= 0x10;
                g_main_system_flags |= 4;
                level->phase = 2;
                level->countdown = 8;
            }
        } else {
            g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
                = 0xA;
        }
    } else {
        if (world_thread_is_running(0xE) != 0) {
            world_thread_set_task_id_to_three(0xE);
        }
        wldcore_sound_enqueue_audio_command(2, 0x20);
        main_sound_stop_weather_sfx_music();
        level->phase = 3;
        g_main_system_flags |= 4;
        if (!(*state_flags & 0x100)) {
            wldcore_fade_start_screen(2, 0x10);
        }
    }
}
