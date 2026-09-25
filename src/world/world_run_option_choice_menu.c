/* Two dead-store conditionals are zero-instruction allocation devices: flow deletes each body,
 * post-reload jump deletes each branch, but both stay visible to register allocation.
 * - `if (height <= 0)`: the sixth height reference (6 refs/54 insns) ties the render
 *   parameter block's priority (17/306); declaring height before param breaks the tie,
 *   giving the target's height s4 / param s5.
 * - `i = 0; if (i < width)`: combine folds `0 < width` into a zero-test branch while keeping
 *   `i = 0`, so the slt result pseudo keeps its flow refs, gets no register, and reload gives
 *   it the target's second unreferenced 8-byte slot (frame 0x60; the text loop's entry test
 *   supplies the first). */
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Option-menu value picker thread for one game option.
 *
 * The option index arrives in the thread's first parameter, which is then
 * replaced by the icon parameter block. The entries are drawn with the stored
 * value highlighted; up/down wrap the cursor, circle stores it and cross
 * cancels. For option index 13 circle only accepts entry 1, which restores
 * g_world_game_option_original_values; any other option clears value 13.
 * On exit the thirteen values are packed back into g_main_game_options, the
 * sound mode is applied on the main stack and the input-repeat timing is
 * reloaded from the finger-cursor speed profile.
 */
void world_run_option_choice_menu(void) {
    RECT rect;
    u32* input;
    native_thread_t* thread;
    s32 kind;
    s32 height;
    world_menu_icon_thread_param_t* param;
    s32 width;
    void* buffer;
    s32 i;
    s32 cursor;
    world_menu_icon_sprites_t* record;

    input = world_input_get_menu_controller(0);
    world_input_get_menu_controller(1);
    thread = &g_world_threads[g_world_thread_current_id];
    kind = thread->function_parameter_1;
    thread->function_parameter_1 = (s32)&g_world_option_menu_icon_thread_param;
    param = &g_world_option_menu_icon_thread_param;
    width = g_world_option_menu_text_widths[kind];
    if (width & 3) {
        width += 2;
    }
    height = g_world_option_menu_entry_counts[kind] * 16 + 16;
    buffer = world_menu_build_and_upload_window_frame_image(width, height, &rect, 1);
    world_thread_wait_frames(1);
    g_world_menu_text_state.stride = width;
    for (i = 0; i < g_world_option_menu_entry_counts[kind]; i++) {
        /* The target passes both coordinates without the s16 sign extension. */
        ((void (*)(s32, s32))world_menu_set_text_origin)(8, i * 16 + 8);
        if (i == g_world_game_option_values[kind]) {
            g_world_menu_text_color = 8;
        }
        world_menu_display_text_entry(
            i + g_world_option_menu_first_text_ids[kind], buffer, &g_world_menu_text_state.origin_x);
        g_world_menu_text_color = 0;
    }
    /* Dead store: allocation device, see the file header. */
    if (height <= 0) {
        cursor = 0;
    }
    param->icon_x = 0x74 - width;
    param->flags = 0x28 - (height >> 1);
    param->width = param->window_width = param->text_image.width = width;
    param->height = param->window_height = param->text_image.height = height;
    param->x = 0x600;
    param->y = 0x700 - height;
    if (kind == 3) {
        param->x = 0x1AE;
    }
    param->text_image.width -= 4;
    world_menu_build_icon_record(&rect, param, &g_world_option_picker_icon_records[0]);
    world_script_copy_bytes(&g_world_option_picker_icon_records[1], &g_world_option_picker_icon_records[0], 0x7C);
    LoadImage(&rect, buffer);
    world_menu_free_memory(buffer);
    world_thread_wait_frames(1);
    cursor = g_world_game_option_values[kind];
    for (width = 0;; width++) {
        world_menu_check_thread_completion((s32*)input);
        record = &g_world_option_picker_icon_records[width & 1].base;
        if (world_menu_is_input_allowed()) {
            if (*input & PSX_PAD_CROSS) {
                g_world_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                break;
            }
            if (*input & PSX_PAD_CIRCLE) {
                g_world_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                if (kind == GAME_OPTION_COUNT) {
                    if (cursor == 1) {
                        /* Dead store: allocation device, see the file header. */
                        i = 0;
                        if (i < width) {
                            record = 0;
                        }
                        for (; i < GAME_OPTION_COUNT; i++) {
                            g_world_game_option_values[i] = g_world_game_option_original_values[i];
                        }
                        g_world_game_option_values[kind] = cursor;
                        break;
                    }
                } else {
                    g_world_game_option_values[kind] = cursor;
                    g_world_game_option_values[GAME_OPTION_UNCHANGED] = 0;
                    break;
                }
            }
            if (*input & PSX_PAD_UP) {
                *input = 0;
                if (cursor != 0) {
                    cursor--;
                } else {
                    cursor = g_world_option_menu_entry_counts[kind] - 1;
                }
                g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            }
            if (*input & PSX_PAD_DOWN) {
                *input = 0;
                if (cursor == g_world_option_menu_entry_counts[kind] - 1) {
                    cursor = 0;
                } else {
                    cursor++;
                }
                g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            }
        }
        world_menu_select_icon_cluts(record);
        world_menu_update_icon_cursor_sprites(param, record, width, cursor);
        world_menu_submit_icon_primitives(record);
        world_thread_wait_frames(1);
    }
    g_main_game_options.fields.cursor_movement = g_world_game_option_values[GAME_OPTION_CURSOR_MOVEMENT];
    g_main_game_options.fields.cursor_repeat_speed = g_world_game_option_values[GAME_OPTION_CURSOR_REPEAT_SPEED];
    g_main_game_options.fields.multi_height_cursor_speed
        = g_world_game_option_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED];
    g_main_game_options.fields.finger_cursor_repeat_speed
        = g_world_game_option_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED];
    g_main_game_options.fields.message_display_speed = g_world_game_option_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED];
    g_main_game_options.fields.navigation_messages = g_world_game_option_values[GAME_OPTION_NAVIGATION_MESSAGES];
    g_main_game_options.fields.ability_names = g_world_game_option_values[GAME_OPTION_ABILITY_NAMES];
    g_main_game_options.fields.effect_messages = g_world_game_option_values[GAME_OPTION_EFFECT_MESSAGES];
    g_main_game_options.fields.display_gained_exp_jp = g_world_game_option_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP];
    g_main_game_options.fields.target_flashing = g_world_game_option_values[GAME_OPTION_TARGET_FLASHING];
    g_main_game_options.fields.show_unequippable_items
        = g_world_game_option_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS];
    g_main_game_options.fields.max_equip_at_job_change
        = g_world_game_option_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE];
    g_main_game_options.fields.sound_mode = g_world_game_option_values[GAME_OPTION_SOUND_MODE];
    g_world_thread_call_target = (void (*)(void))main_sound_set_type;
    world_thread_call_on_main_stack(g_world_game_option_values[GAME_OPTION_SOUND_MODE]);
    width = g_main_game_options.fields.finger_cursor_repeat_speed;
    g_main_input_repeat_initial_delay = g_world_option_input_timing_profiles[width].initial_delay;
    g_main_input_repeat_period = g_world_option_input_timing_profiles[width].repeat_period;
    g_main_input_secondary_repeat_period = g_world_option_input_timing_profiles[width].secondary_repeat_period;
    g_main_menu_scroll_accel_delay = g_world_option_input_timing_profiles[width].scroll_accel_delay;
    g_main_menu_scroll_slow_step = g_world_option_input_timing_profiles[width].scroll_slow_step;
    g_main_menu_scroll_fast_step = g_world_option_input_timing_profiles[width].scroll_fast_step;
    *input = 0;
    world_thread_wait_frames(1);
    world_thread_exit_current();
}
