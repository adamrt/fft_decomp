#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Advance the system flags, reset the script variables and stop this thread. */
void world_script_reset_event_variables_thread(void) {
    g_main_system_flags += 3;
    script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 1);
    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, 0);
    script_set_variable(EVENT_SCRIPT_VAR_WEATHER, 0);
    script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, 0);
    g_wldcore_next_map_id[0] = g_world_menu_number_entry_value;
    thread_exit_current();
}

/* Sibling of world_card_update_menu_request driven by g_world_formation_open_request_state. */
void world_formation_update_open_request(void) {
    s16 state = g_world_formation_open_request_state;
    s32 i;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_formation_open_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_formation_open_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_RUN) {
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        world_formation_run_screen(0, 0);
        world_formation_reset_menu_context();
        g_world_menu_screen_open_request = 0;
        world_menu_start_thread_group(g_world_menu_thread_group_parameter, g_world_menu_location_id,
            g_world_menu_location_window_x, g_world_menu_location_window_y);
        world_menu_open_entry_window(g_world_menu_open_entry_index, -0x30, -0x38);
        g_menu_thread_menu_data = g_world_menu_default_entries;
        world_formation_save_and_set_system_flags();
        wldcore_clear_screen_and_set_map_clip_rect(0);
        for (i = 0x77FF; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
        g_world_formation_open_request_state++;
        world_menu_set_brightness(0, 0, 0);
        return;
    }
    if (state < 8) {
        g_world_formation_open_request_state = state + 1;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_IN) {
        world_formation_restore_saved_system_flags();
        wldcore_fade_start_screen(0x24, 0x10);
        g_world_formation_open_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_IN) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_formation_open_request_state = WORLD_SCREEN_REQUEST_IDLE;
        }
    }
}

void world_formation_run_screen_for_menu_index(s32 menu_index) {
    world_formation_reset_menu_context();
    world_formation_run_screen(menu_index + 1, 0);
    world_formation_reset_menu_context();
    g_world_menu_screen_open_request = 0;
    g_menu_thread_menu_data = g_world_menu_default_entries;
}

/* Steps a pending request to open the tutorial categories from the world
 * map: start the screen fade-out, wait for it, then reset the menu context
 * and raise MAIN_SYSTEM_FLAG_REQUEST_TUTORIAL. */
void world_formation_update_tutorial_categories_request(void) {
    s16 state = g_world_tutorial_categories_request_state;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_tutorial_categories_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_tutorial_categories_request_state = state + 1;
        }
        return;
    }
    world_formation_reset_menu_context();
    world_menu_set_brightness(0x80, 0x80, 0x80);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_TUTORIAL;
    g_world_tutorial_categories_request_state = WORLD_SCREEN_REQUEST_IDLE;
}

/* Serve a pending request to open the debug variable list from the world
 * map: reset the menu context and raise
 * MAIN_SYSTEM_FLAG_REQUEST_VARIABLE_LIST. */
void world_formation_update_debug_variable_list_request(void) {
    s32 state = g_world_debug_variable_list_request_state;

    if (state != 0 && state == 1) {
        g_world_menu_screen_open_request = 1;
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        g_world_debug_variable_list_request_state = 0;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_VARIABLE_LIST;
    }
}

/* Serve a pending request to open the fixed number panel from the world
 * map: reset the menu context, reload and relocate the menu text file and
 * raise MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL. */
void world_formation_update_fixed_number_panel_request(void) {
    s32 state = g_world_fixed_number_panel_request_state;

    if (state != 0 && state == 1) {
        g_world_menu_screen_open_request = 1;
        world_formation_reset_menu_context();
        g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xE800, g_world_menu_text_file_buffer);
        text_relocate_pointer_table(g_world_menu_text_file_buffer);
        world_menu_set_brightness(0x80, 0x80, 0x80);
        g_world_fixed_number_panel_request_state = 0;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL;
    }
}

/* Sibling of world_formation_update_tutorial_categories_request driven by g_world_card_menu_request_state. */
void world_card_update_menu_request(void) {
    s16 state = g_world_card_menu_request_state;
    s32 result;
    s32 i;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_card_menu_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_card_menu_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_RUN) {
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        result = world_card_run_menu_screen(1);
        world_formation_reset_menu_context();
        g_world_menu_screen_open_request = 0;
        g_menu_thread_menu_data = g_world_menu_default_entries;
        world_formation_save_and_set_system_flags();
        if (result == 1) {
            g_world_card_menu_request_state = WORLD_SCREEN_REQUEST_IDLE;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_RESTART;
            return;
        }
        world_menu_start_thread_group(g_world_menu_thread_group_parameter, g_world_menu_location_id,
            g_world_menu_location_window_x, g_world_menu_location_window_y);
        world_menu_open_entry_window(g_world_menu_open_entry_index, -0x30, -0x38);
        wldcore_clear_screen_and_set_map_clip_rect(0);
        for (i = 0x77FF; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
        g_world_card_menu_request_state++;
        world_menu_set_brightness(0, 0, 0);
        return;
    }
    if (state < 8) {
        g_world_card_menu_request_state = state + 1;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_IN) {
        world_formation_restore_saved_system_flags();
        wldcore_fade_start_screen(0x24, 0x10);
        g_world_card_menu_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_IN) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_card_menu_request_state = WORLD_SCREEN_REQUEST_IDLE;
        }
    }
}

/* Sibling of world_formation_update_tutorial_categories_request driven by g_world_map_root_request_state. */
void world_formation_update_world_map_root_request(void) {
    s16 state = g_world_map_root_request_state;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_map_root_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_map_root_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_RUN) {
        world_formation_reset_menu_context();
        g_world_map_root_request_state = WORLD_SCREEN_REQUEST_IDLE;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_BRAVE_STORY;
    }
}

/* Records the location window request and starts the menu thread-group supervisor on thread 2, unless a
 * menu screen is already opening. */
void world_menu_start_thread_group(s32 parameter, s32 location_id, s32 window_x, s32 window_y) {
    if (g_world_menu_screen_open_request == 0) {
        if (g_world_menu_location_id != location_id) {
            g_threads[12].task_id = THREAD_TASK_NONE;
        }
        g_world_menu_location_text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_world_menu_thread_group_parameter = parameter;
        g_world_menu_location_id = location_id;
        g_world_menu_location_window_x = window_x;
        g_world_menu_location_window_y = window_y;
        g_world_menu_default_entries[2].text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_world_menu_default_entries[2].window.x = window_x;
        g_world_menu_default_entries[2].window.y = window_y;
        thread_start(WORLD_THREAD_SLOT_MENU_GROUP, world_menu_thread_group_supervisor_thread);
        thread_set_parameters(WORLD_THREAD_SLOT_MENU_GROUP, parameter, 0, 0);
    }
}

/* Menu thread-group dispatcher thread. Each request in parameter 1 selects a
 * group: running members with the expected task are signaled to continue,
 * others are signaled to finish (state 1) and awaited, then every finished
 * or idle member is restarted with its function, the matching menu entry, and
 * its task id. The thread exits once no further request is pending. */
void world_menu_thread_group_supervisor_thread(void) {
    s32 state[16];
    world_menu_thread_group_t* group;
    s32 index;
    s32 i;
    s32 thread_id;

    for (;;) {
        index = g_threads[g_thread_current_id].function_parameter_1;
        g_threads[g_thread_current_id].function_parameter_1 = 0;
        if (index >= 4) {
            index = 0;
        }
        group = &g_world_menu_thread_groups[index];
        for (i = 15; i >= 0; i--) {
            state[i] = 0;
        }
        for (i = 0; i < 3; i++) {
            thread_id = group->thread_id[i];
            if (thread_id != -1) {
                if (g_threads[thread_id].is_running != 0) {
                    if (g_threads[thread_id].task_id != group->task_id[i]) {
                        state[thread_id] = 1;
                        thread_set_parameters(group->thread_id[i], 0, 0, 1);
                    } else {
                        thread_set_parameters(thread_id, 0, 1, 0);
                    }
                } else {
                    state[thread_id] = 2;
                }
            }
        }
        for (;;) {
            for (i = 0; i < 3; i++) {
                if (state[group->thread_id[i]] == 1 && g_threads[group->thread_id[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 3) {
                break;
            }
            thread_wait_frames(1);
        }
        for (i = 0; i < 3; i++) {
            if (state[group->thread_id[i]] != 0 && group->function[i] != (void (*)(void))-1) {
                thread_start(group->thread_id[i], group->function[i]);
                thread_set_parameters(group->thread_id[i], (s32)&g_menu_thread_menu_data[i], 0, 0);
                g_threads[group->thread_id[i]].task_id = group->task_id[i];
            }
        }
        if (g_threads[g_thread_current_id].function_parameter_1 == 0) {
            thread_exit_current();
        }
    }
}

/* Set the text image's upload rectangle and initialize its sprite. */
void world_menu_init_quad_from_record(RECT* out, const menu_entry_t* record, SPRT* primitive) {
    out->x = record->vram_origin.x;
    out->y = record->vram_origin.y;
    out->w = record->inner_width >> 2;
    out->h = record->inner_height;
    menu_init_sprt_variant(primitive);
    primitive->clut = 0x7C3C;
    gfx_init_image_loading(primitive, &record->vram_origin, &record->window, &record->text_image);
}

/* Set up the two menu sprites for one record. The first sprite is positioned
 * from the record's x/y according to its layout mode (1 or 2), the second is
 * a fixed 16x16 sprite. Both use CLUT 0x7d7c.
 *
 * `mode` is reused to carry the y coordinate once the mode test is done; that
 * reuse is what puts the y value in the target's v1. */
void world_menu_build_layout_sprites(menu_entry_t* entry, SPRT sprite[2], s32 unused_2) {
    u16 x;
    s32 mode;

    menu_init_sprt_variant(sprite);
    sprite->clut = 0x7D7C;
    mode = entry->header_id;
    if (mode == 1) {
        x = entry->window.x;
        sprite->x0 = x;
        mode = (u16)entry->window.y;
        sprite->u0 = 0xA8;
        sprite->v0 = 0x88;
        sprite->w = 0x28;
        sprite->h = 8;
        mode -= 1;
        sprite->y0 = mode;
    } else {
        if (mode == 2) {
            x = entry->window.x;
            sprite->x0 = x - 6;
            mode = (u16)entry->window.y;
            sprite->u0 = 0xA8;
            sprite->v0 = 0x78;
            sprite->w = 0x28;
            sprite->h = 0x10;
            mode -= 6;
            sprite->y0 = mode;
        }
    }
    sprite++;
    menu_init_sprt_variant(sprite);
    sprite->w = 0x10;
    sprite->h = 0x10;
    sprite->clut = 0x7D7C;
}

void world_formation_set_menu_allowed_buttons(u32 mask) {
    g_world_formation_menu_allowed_buttons = mask;
}

/* The six 32-bit input masks at 0x801aed44 are g_world_formation_menu_input_state,
 * but a struct global makes GCC keep a base register for the field accesses
 * (see world_input_update_event_state), so they are separate scalars here. */
extern u32 g_world_formation_menu_current_input, g_world_formation_menu_previous_input,
    g_world_formation_menu_initial_repeat_counter, g_world_formation_menu_repeat_counter,
    g_world_formation_menu_secondary_repeat_counter;

/* Per-frame formation-menu step: loads the input timing profile selected by
 * customized-options bits 9-11, derives the edge/auto-repeat 32-bit input mask
 * (masked by the formation entry mask and cleared while any of the six
 * g_world_formation_open_request_state.. flags is set), runs the formation/memory-card request
 * updates, publishes the input, yields once while tracking thread status
 * changes and returns the menu result slot g_menu_pending_selection[0] (-2 while pending).
 * Formation-menu sibling of world_input_update_event_state. */
s16 world_formation_run_menu_frame(u32* otag, u32 input) {
    s32 profile;
    u32 previous_input;
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_delay;
    u32 counter;
    s32 thread_id;
    s32 status;

    profile = g_main_game_options.fields.finger_cursor_repeat_speed;
    g_main_input_repeat_initial_delay = g_world_input_timing_profiles[profile].initial_delay;
    g_main_input_repeat_period = g_world_input_timing_profiles[profile].repeat_period;
    g_main_input_secondary_repeat_period = g_world_input_timing_profiles[profile].secondary_repeat_period;
    g_main_menu_scroll_accel_delay = g_world_input_timing_profiles[profile].scroll_accel_delay;
    g_main_menu_scroll_slow_step = g_world_input_timing_profiles[profile].scroll_slow_step;
    g_main_menu_scroll_fast_step = g_world_input_timing_profiles[profile].scroll_fast_step;
    if (input == 0) {
        g_world_formation_menu_initial_repeat_counter = 0;
        g_world_formation_menu_repeat_counter = 0;
        g_world_formation_menu_secondary_repeat_counter = 0;
    }

    previous_input = g_world_formation_menu_current_input;
    g_world_formation_menu_current_input = input;
    g_world_formation_menu_new_buttons = 0;
    g_world_formation_menu_previous_input = previous_input;

    if (previous_input != input || (filtered_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_world_formation_menu_new_buttons = ~previous_input & input;
        g_world_formation_menu_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_world_formation_menu_initial_repeat_counter;
        if (counter == initial_delay) {
            g_world_formation_menu_new_buttons = filtered_input;
        }
        if (counter >= initial_delay) {
            repeat_delay = g_main_input_repeat_period / g_script_event_speed;
            counter = ++g_world_formation_menu_repeat_counter;
            g_world_formation_menu_secondary_repeat_counter++;
            if (counter >= repeat_delay) {
                g_world_formation_menu_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_world_formation_menu_new_buttons = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_world_formation_menu_new_buttons = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_delay = g_main_input_secondary_repeat_period / g_script_event_speed;
            if (g_world_formation_menu_secondary_repeat_counter >= secondary_delay) {
                g_world_formation_menu_secondary_repeat_counter = 0;
                if ((g_world_formation_menu_current_input & PSX_PAD_SQUARE) != 0) {
                    g_world_formation_menu_new_buttons
                        |= g_world_formation_menu_current_input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_world_formation_menu_new_buttons
                        |= g_world_formation_menu_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_world_formation_menu_repeat_counter = 0;
            g_world_formation_menu_secondary_repeat_counter = 0;
        }
    }

    g_world_formation_menu_new_buttons &= g_world_formation_menu_allowed_buttons;
    if (g_world_formation_open_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_tutorial_categories_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_debug_variable_list_request_state != 0) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_card_menu_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_map_root_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_fixed_number_panel_request_state != 0) {
        g_world_formation_menu_new_buttons = 0;
    }
    g_formation_screen_active = 1;
    world_formation_update_open_request();
    world_formation_update_world_map_root_request();
    world_card_update_menu_request();
    world_formation_update_tutorial_categories_request();
    world_formation_update_debug_variable_list_request();
    world_formation_update_fixed_number_panel_request();
    g_menu_pending_selection[0] = -2;
    g_gfx_current_otag_entry = otag;
    g_sound_pending_sfx_id = -1;
    menu_draw_active_window_frames_variant();
    script_load_event_input_state(&g_world_formation_menu_input_state);
    if (g_world_formation_menu_new_buttons != 0x100 && g_world_formation_menu_new_buttons != 0) {
        g_script_input_suppression_frames = 5;
    }

    for (thread_id = 1; thread_id < 16; thread_id++) {
        g_world_formation_thread_status_snapshot[thread_id] = thread_is_running_asm_variant(thread_id);
    }

    thread_yield();
    for (thread_id = 1; thread_id < 16; thread_id++) {
        status = g_world_formation_thread_status_snapshot[thread_id];
        if (status != thread_is_running_asm_variant(thread_id)) {
            g_script_input_suppression_frames = 5;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }
    if (g_menu_companion_executable_request_index != 0) {
        g_menu_companion_executable_request_index = 0;
    }
    g_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_sound_pending_sfx_id != -1) {
        main_sound_play_sfx_find_channel(g_sound_pending_sfx_id);
    }
    if (g_menu_pending_selection[0] == 0) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, g_menu_pending_selection[1]);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_DESTINATION;
    }
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
    return g_menu_pending_selection[0];
}
