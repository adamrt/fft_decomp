#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Labeled numeric entry thread: world_menu_number_entry_thread with a text label.
 *
 * Draws param->text_id left of the number and edits the 16-bit value at
 * param->value one decimal digit at a time within the range bound at param->context. The value is
 * written back each frame only while g_world_formation_screen_active is set. The initial label is
 * measured in pixels and each redraw in character columns (times 6).
 * i is shared by the quad setup loop and the frame loop, as in the target.
 */
void world_menu_labeled_number_entry_thread(void) {
    RECT rect;
    menu_icon_record_t record;
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    s16 label_size[2];
    world_menu_number_range_t* range;
    u8* buffer;
    s16 original;
    s32 digits;
    thread_t* thread;
    menu_entry_t* param;
    s16 value;
    s32 i;
    s32 digit;
    s32 redraw;
    u32 input;
    POLY_FT4* cursor_quad;
    POLY_FT4* shadow_quad;

    g_world_menu_list_controller_input = script_get_controller_input_pointer(0);
    thread = &g_threads[g_thread_current_id];
    param = *(menu_entry_t**)thread;
    range = param->context;
    digits = text_count_decimal_digits(range->bounds.max);
    original = *menu_entry_result(param);
    value = original;
    if (value < range->bounds.min || range->bounds.max < value) {
        value = range->bounds.min;
    }
    menu_build_window_sprites_variant(&rect, param, &record);
    for (i = 0; i < 2; i++) {
        menu_init_default_poly_ft4(&cursor_quads[i]);
        SetSemiTrans(&cursor_quads[i], 0);
        cursor_quads[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        cursor_quads[i].clut = 0x7D7C;
        cursor_quads[i].u0 = 0xB7;
        cursor_quads[i].u1 = 0xB7;
        cursor_quads[i].u2 = 0xA7;
        cursor_quads[i].u3 = 0xA7;
        cursor_quads[i].v0 = 0;
        cursor_quads[i].v1 = 0x10;
        cursor_quads[i].v2 = 0;
        cursor_quads[i].v3 = 0x10;
        menu_init_default_poly_ft4(&shadow_quads[i]);
        SetSemiTrans(&shadow_quads[i], 1);
        shadow_quads[i].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        shadow_quads[i].clut = 0x7DBC;
        shadow_quads[i].u0 = 0xC7;
        shadow_quads[i].u1 = 0xC7;
        shadow_quads[i].u2 = 0xB7;
        shadow_quads[i].u3 = 0xB7;
        shadow_quads[i].v0 = 0;
        shadow_quads[i].v1 = 0x10;
        shadow_quads[i].v2 = 0;
        shadow_quads[i].v3 = 0x10;
    }
    digit = 0;
    buffer = menu_build_and_upload_window_frame_image_variant(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    text_measure_pixels(&label_size[0], &label_size[1], text_find_entry(param->text_id));
    menu_set_text_origin(8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    menu_set_text_origin((u16)label_size[0] + 8, 9);
    menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
    world_gfx_draw_signed_number(value, digits, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, buffer);
    redraw = 1;
    for (i = 0;; i++) {
        if (g_world_formation_screen_active != 0) {
            *menu_entry_result(param) = value;
        }
        thread_wait_frames(1);
        if (redraw == 1) {
            menu_free_memory(buffer);
            redraw = 0;
        }
        if (menu_should_close_thread_variant(g_world_menu_list_controller_input) != 0) {
            break;
        }
        input = *g_world_menu_list_controller_input;
        menu_handle_cancel_input(param);
        menu_handle_action(param, 0);
        if (input != *g_world_menu_list_controller_input) {
            if (*menu_entry_result(param) == -1) {
                *menu_entry_result(param) = original;
            } else {
                world_menu_number_range_t* current_range;

                *menu_entry_result(param) = value;
                current_range = param->context;
                current_range->on_change();
            }
        }
        if (thread[-1].is_running == 0) {
            if ((*g_world_menu_list_controller_input & PSX_PAD_RIGHT) && digit > 0) {
                digit--;
            }
            if ((*g_world_menu_list_controller_input & PSX_PAD_LEFT) && digit < digits - 1) {
                digit++;
            }
            if (*g_world_menu_list_controller_input & PSX_PAD_UP) {
                if (value <= range->bounds.max - g_world_menu_number_entry_digit_steps[digit]) {
                    value += g_world_menu_number_entry_digit_steps[digit];
                } else {
                    value = range->bounds.max;
                }
                redraw = 1;
            }
            if (*g_world_menu_list_controller_input & PSX_PAD_DOWN) {
                if (value >= range->bounds.min + g_world_menu_number_entry_digit_steps[digit]) {
                    value -= g_world_menu_number_entry_digit_steps[digit];
                } else {
                    value = range->bounds.min;
                }
                redraw = 1;
            }
        }
        if (redraw == 1) {
            buffer = menu_build_and_upload_window_frame_image_variant(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            g_menu_text_state.pen.stride = param->inner_width;
            text_measure(&label_size[0], &label_size[1], text_find_entry(param->text_id));
            menu_set_text_origin(8, 9);
            menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
            menu_set_text_origin((u16)label_size[0] * 6 + 8, 9);
            menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
            world_gfx_draw_signed_number(value, digits, buffer, &g_menu_text_state.pen);
            LoadImage(&rect, buffer);
        }
        cursor_quad = &cursor_quads[i & 1];
        shadow_quad = &shadow_quads[i & 1];
        world_menu_update_entry_cursor_quads(param, cursor_quad, shadow_quad, i, digit);
        menu_select_sprite_clut(cursor_quad);
        menu_select_sprite_clut(shadow_quad);
        if (thread_is_previous_running_variant() == 0) {
            cursor_quad->clut = 0x7D7C;
            shadow_quad->clut = 0x7DBC;
        } else {
            cursor_quad->clut = 0x7DFC;
            shadow_quad->clut = 0x7E3C;
        }
        menu_update_icon_cursor_sprites(param, &record.base, i, -1);
        menu_select_icon_cluts(&record.base);
        gfx_draw_or_append_gpu_primitive(&cursor_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&shadow_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&record.base.sprites[0]);
        gfx_draw_or_append_gpu_primitive(&record);
    }
    thread_yield();
    thread_exit_current();
}
