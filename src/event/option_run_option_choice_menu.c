/* Run the option choice menu.
 *
 * Port of world_run_option_choice_menu (identical apart from the four window constants).
 * Two dead-store conditionals are zero-instruction allocation devices: flow deletes each body,
 * post-reload jump deletes each branch, but both stay visible to register allocation.
 * - `if (height <= 0)`: the sixth height reference (6 refs/54 insns) ties the render
 *   parameter block's priority (17/306); declaring height before param breaks the tie,
 *   giving the target's height s4 / param s5.
 * - `i = 0; if (i < width)`: combine folds `0 < width` into a zero-test branch while keeping
 *   `i = 0`, so the slt result pseudo keeps its flow refs, gets no register, and reload gives
 *   it the target's second unreferenced 8-byte slot (frame 0x60; the text loop's entry test
 *   supplies the first). */
#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/option.h"
#include "fft/options.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern world_input_timing_profile_t g_option_input_repeat_timings[];

void option_run_option_choice_menu(void) {
    RECT rect;
    u32* input;
    native_thread_t* thread;
    s32 kind;
    s32 height;
    world_menu_icon_thread_param_t* param;
    s32 width;
    u32* buffer;
    s32 i;
    s32 cursor;
    battle_menu_window_record_t* record;

    input = battle_script_get_controller_input_pointer(0);
    battle_script_get_controller_input_pointer(1);
    thread = &g_battle_threads[g_battle_current_thread_id];
    kind = thread->function_parameter_1;
    thread->function_parameter_1 = (s32)&g_option_menu_icon_thread_param;
    param = &g_option_menu_icon_thread_param;
    width = g_option_choice_window_widths[kind];
    if (width & 3) {
        width += 2;
    }
    height = g_option_choice_counts[kind] * 16 + 16;
    buffer = battle_menu_build_and_upload_window_frame_image(width, height, &rect, 1);
    battle_thread_wait_frames(1);
    g_menu_text_state.stride = width;
    for (i = 0; i < g_option_choice_counts[kind]; i++) {
        /* The target passes both coordinates without the s16 sign extension. */
        ((void (*)(s32, s32))battle_menu_set_text_origin)(8, i * 16 + 8);
        if (i == g_option_current_values[kind]) {
            g_menu_text_palette_offset = 8;
        }
        battle_menu_display_text_entry(i + g_option_value_text_id_bases[kind], buffer, &g_menu_text_state.origin_x);
        g_menu_text_palette_offset = 0;
    }
    /* Dead store: allocation device, see the file header. */
    if (height <= 0) {
        cursor = 0;
    }
    param->icon_x = 0x170 - width;
    param->flags = 0xA0 - (height >> 1);
    param->width = param->window_width = param->text_image.width = width;
    param->height = param->window_height = param->text_image.height = height;
    param->x = 0x1C0;
    param->y = 0x100 - height;
    if (kind == 3) {
        param->x = 0x1AE;
    }
    param->text_image.width -= 4;
    battle_menu_build_window_sprites(
        (battle_menu_window_header_t*)&rect, (battle_menu_window_spec_t*)param, &g_option_picker_icon_records[0]);
    battle_copy_bytes(&g_option_picker_icon_records[1], &g_option_picker_icon_records[0], sizeof(*record));
    LoadImage(&rect, buffer);
    battle_menu_free_memory(buffer);
    battle_thread_wait_frames(1);
    cursor = g_option_current_values[kind];
    for (width = 0;; width++) {
        battle_menu_should_close_thread((s32*)input);
        record = &g_option_picker_icon_records[width & 1];
        if (battle_menu_can_accept_input()) {
            if (*input & PSX_PAD_CROSS) {
                g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                break;
            }
            if (*input & PSX_PAD_CIRCLE) {
                g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                if (kind == GAME_OPTION_COUNT) {
                    if (cursor == 1) {
                        /* Dead store: allocation device, see the file header. */
                        i = 0;
                        if (i < width) {
                            record = 0;
                        }
                        for (; i < GAME_OPTION_COUNT; i++) {
                            g_option_current_values[i] = g_option_reference_values[i];
                        }
                        g_option_current_values[kind] = cursor;
                        break;
                    }
                } else {
                    g_option_current_values[kind] = cursor;
                    g_option_current_values[GAME_OPTION_UNCHANGED] = 0;
                    break;
                }
            }
            if (*input & PSX_PAD_UP) {
                *input = 0;
                if (cursor != 0) {
                    cursor--;
                } else {
                    cursor = g_option_choice_counts[kind] - 1;
                }
                g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            }
            if (*input & PSX_PAD_DOWN) {
                *input = 0;
                if (cursor == g_option_choice_counts[kind] - 1) {
                    cursor = 0;
                } else {
                    cursor++;
                }
                g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            }
        }
        battle_menu_configure_frame_cluts(record);
        battle_update_menu_cursor_primitives(param, (world_menu_icon_sprites_t*)record, width, cursor);
        battle_menu_submit_frame_primitives(record);
        battle_thread_wait_frames(1);
    }
    g_main_game_options.fields.cursor_movement = g_option_current_values[GAME_OPTION_CURSOR_MOVEMENT];
    g_main_game_options.fields.cursor_repeat_speed = g_option_current_values[GAME_OPTION_CURSOR_REPEAT_SPEED];
    g_main_game_options.fields.multi_height_cursor_speed
        = g_option_current_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED];
    g_main_game_options.fields.finger_cursor_repeat_speed
        = g_option_current_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED];
    g_main_game_options.fields.message_display_speed = g_option_current_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED];
    g_main_game_options.fields.navigation_messages = g_option_current_values[GAME_OPTION_NAVIGATION_MESSAGES];
    g_main_game_options.fields.ability_names = g_option_current_values[GAME_OPTION_ABILITY_NAMES];
    g_main_game_options.fields.effect_messages = g_option_current_values[GAME_OPTION_EFFECT_MESSAGES];
    g_main_game_options.fields.display_gained_exp_jp = g_option_current_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP];
    g_main_game_options.fields.target_flashing = g_option_current_values[GAME_OPTION_TARGET_FLASHING];
    g_main_game_options.fields.show_unequippable_items = g_option_current_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS];
    g_main_game_options.fields.max_equip_at_job_change = g_option_current_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE];
    g_main_game_options.fields.sound_mode = g_option_current_values[GAME_OPTION_SOUND_MODE];
    g_battle_thread_call_target = (void (*)(void))main_sound_set_type;
    battle_thread_call_on_main_stack(g_option_current_values[GAME_OPTION_SOUND_MODE]);
    width = g_main_game_options.fields.finger_cursor_repeat_speed;
    g_main_input_repeat_initial_delay = g_option_input_repeat_timings[width].initial_delay;
    g_main_input_repeat_period = g_option_input_repeat_timings[width].repeat_period;
    g_main_input_secondary_repeat_period = g_option_input_repeat_timings[width].secondary_repeat_period;
    g_main_menu_scroll_accel_delay = g_option_input_repeat_timings[width].scroll_accel_delay;
    g_main_menu_scroll_slow_step = g_option_input_repeat_timings[width].scroll_slow_step;
    g_main_menu_scroll_fast_step = g_option_input_repeat_timings[width].scroll_fast_step;
    *input = 0;
    battle_thread_wait_frames(1);
    battle_thread_exit_current();
}
