/* The option menu's value picker thread for one game option. */

#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
/* Two dead-store conditionals are zero-instruction allocation devices: flow deletes each body,
 * post-reload jump deletes each branch, but both stay visible to register allocation.
 * - `if (height <= 0)`: the sixth height reference (6 refs/54 insns) ties the render
 *   parameter block's priority (17/306); declaring height before param breaks the tie,
 *   giving the target's height s4 / param s5.
 * - `i = 0; if (i < width)`: combine folds `0 < width` into a zero-test branch while keeping
 *   `i = 0`, so the slt result pseudo keeps its flow refs, gets no register, and reload gives
 *   it the target's second unreferenced 8-byte slot (frame 0x60; the text loop's entry test
 *   supplies the first). */
#include "fft/main.h"
#include "fft/menu_input.h"
#include "fft/menu_overlay.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Option-menu value picker thread for one game option.
 *
 * The option index arrives in the thread's first parameter, which is then
 * replaced by the icon parameter block. The entries are drawn with the stored
 * value highlighted; up/down wrap the cursor, circle stores it and cross
 * cancels. For option index 13 circle only accepts entry 1, which restores
 * g_menu_option_reference_values; any other option clears value 13.
 * On exit the thirteen values are packed back into g_main_game_options, the
 * sound mode is applied on the main stack and the input-repeat timing is
 * reloaded from the finger-cursor speed profile.
 */
void menu_run_option_choice(void) {
    RECT rect;
    u32* input = script_get_controller_input_pointer(0);
    thread_t* thread;
    s32 kind;
    s32 height;
    menu_entry_t* param;
    s32 width;
    u8* buffer;
    s32 i;
    s32 cursor;
    menu_icon_sprites_t* record;

    script_get_controller_input_pointer(1);
    thread = &g_threads[g_thread_current_id];
    kind = thread->function_parameter_1;
    thread->function_parameter_1 = (s32)&g_menu_option_icon_thread_param;
    param = &g_menu_option_icon_thread_param;

    /* The 4-bit text image must span whole VRAM halfwords (4 pixels each); adding 2 aligns a
     * width of 4n + 2, but would leave an odd width unaligned. */
    width = g_menu_option_choice_widths[kind];
    if (width & 3) {
        width += 2;
    }
    height = g_menu_option_choice_counts[kind] * 16 + 16;
    buffer = menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    thread_wait_frames(1);

    g_menu_text_state.pen.stride = width;
    for (i = 0; i < g_menu_option_choice_counts[kind]; i++) {
        menu_set_text_origin(8, i * 16 + 8);
        if (i == g_menu_option_values[kind]) {
            g_menu_text_palette_offset = 8;
        }
        menu_display_text_entry(i + g_menu_option_value_text_id_bases[kind], buffer, &g_menu_text_state.pen);
        g_menu_text_palette_offset = 0;
    }

    /* Dead store: allocation device, see the file header. */
    if (height <= 0) {
        cursor = 0;
    }

    /* Window placement and VRAM staging origin differ per program: OPTION
     * (over the battle screen) uses 368/160 and 448/256, WORLD 116/40 and
     * 1536/1792. */
    param->window.x = VARIANT_MENU_CHOICE_WINDOW_RIGHT_X - width;
    param->window.y = VARIANT_MENU_CHOICE_WINDOW_CENTER_Y - (height >> 1);
    param->inner_width = param->window.w = param->text_image.width = width;
    param->inner_height = param->window.h = param->text_image.height = height;
    param->vram_origin.x = VARIANT_MENU_CHOICE_VRAM_X;
    param->vram_origin.y = VARIANT_MENU_CHOICE_VRAM_BOTTOM_Y - height;
    if (kind == GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED) {
        param->vram_origin.x = 0x1AE;
    }
    param->text_image.width -= 4;

    menu_build_window_sprites(&rect, param, &g_menu_option_picker_icon_records[0]);
    /* The loop draws from the two icon records alternately. */
    mem_copy_bytes_nonempty(&g_menu_option_picker_icon_records[1], &g_menu_option_picker_icon_records[0],
        sizeof(g_menu_option_picker_icon_records[1]));

    LoadImage(&rect, buffer);
    menu_free_memory(buffer);
    thread_wait_frames(1);

    cursor = g_menu_option_values[kind];
    /* `width` is reused as the frame counter. */
    for (width = 0;; width++) {
        menu_should_close_thread(input);
        record = &g_menu_option_picker_icon_records[width & 1].base;
        if (menu_can_accept_input()) {
            if (*input & PSX_PAD_CROSS) {
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                break;
            }

            if (*input & PSX_PAD_CIRCLE) {
                g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                /* Row 13 is the defaults entry: choice 1 restores every option to its reference value. */
                if (kind == GAME_OPTION_COUNT) {
                    if (cursor == 1) {
                        /* Dead store: allocation device, see the file header. */
                        i = 0;
                        if (i < width) {
                            record = 0;
                        }
                        for (; i < GAME_OPTION_COUNT; i++) {
                            g_menu_option_values[i] = g_menu_option_reference_values[i];
                        }
                        g_menu_option_values[kind] = cursor;
                        break;
                    }
                } else {
                    g_menu_option_values[kind] = cursor;
                    /* Changing any other option clears the defaults entry. */
                    g_menu_option_values[GAME_OPTION_UNCHANGED] = 0;
                    break;
                }
            }

            if (*input & PSX_PAD_UP) {
                *input = 0;
                if (cursor != 0) {
                    cursor--;
                } else {
                    cursor = g_menu_option_choice_counts[kind] - 1;
                }
                g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
            }

            if (*input & PSX_PAD_DOWN) {
                *input = 0;
                if (cursor == g_menu_option_choice_counts[kind] - 1) {
                    cursor = 0;
                } else {
                    cursor++;
                }
                g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
            }
        }

        menu_select_icon_cluts(record);
        menu_update_icon_cursor_sprites(param, record, width, cursor);
        menu_submit_icon_primitives(record);
        thread_wait_frames(1);
    }

    g_main_game_options.fields.cursor_movement = g_menu_option_values[GAME_OPTION_CURSOR_MOVEMENT];
    g_main_game_options.fields.cursor_repeat_speed = g_menu_option_values[GAME_OPTION_CURSOR_REPEAT_SPEED];
    g_main_game_options.fields.multi_height_cursor_speed = g_menu_option_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED];
    g_main_game_options.fields.finger_cursor_repeat_speed
        = g_menu_option_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED];
    g_main_game_options.fields.message_display_speed = g_menu_option_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED];
    g_main_game_options.fields.navigation_messages = g_menu_option_values[GAME_OPTION_NAVIGATION_MESSAGES];
    g_main_game_options.fields.ability_names = g_menu_option_values[GAME_OPTION_ABILITY_NAMES];
    g_main_game_options.fields.effect_messages = g_menu_option_values[GAME_OPTION_EFFECT_MESSAGES];
    g_main_game_options.fields.display_gained_exp_jp = g_menu_option_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP];
    g_main_game_options.fields.target_flashing = g_menu_option_values[GAME_OPTION_TARGET_FLASHING];
    g_main_game_options.fields.show_unequippable_items = g_menu_option_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS];
    g_main_game_options.fields.max_equip_at_job_change = g_menu_option_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE];
    g_main_game_options.fields.sound_mode = g_menu_option_values[GAME_OPTION_SOUND_MODE];
    g_thread_call_target = (void (*)(void))suzuki_sound_set_type;
    thread_call_on_main_stack(g_menu_option_values[GAME_OPTION_SOUND_MODE]);

    /* `width` is reused as the input-repeat timing profile index. */
    width = g_main_game_options.fields.finger_cursor_repeat_speed;
    g_main_input_repeat_initial_delay = g_menu_option_input_repeat_timings[width].initial_delay;
    g_main_input_repeat_period = g_menu_option_input_repeat_timings[width].repeat_period;
    g_main_input_secondary_repeat_period = g_menu_option_input_repeat_timings[width].secondary_repeat_period;
    g_main_menu_scroll_accel_delay = g_menu_option_input_repeat_timings[width].scroll_accel_delay;
    g_main_menu_scroll_slow_step = g_menu_option_input_repeat_timings[width].scroll_slow_step;
    g_main_menu_scroll_fast_step = g_menu_option_input_repeat_timings[width].scroll_fast_step;

    /* Clear the closing press so the options list thread does not act on it too. */
    *input = 0;
    thread_wait_frames(1);
    thread_exit_current();
}
