#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Numeric entry thread: edits the 16-bit value at param->value one decimal digit
 * at a time within the range bound at param->context. Left/right move the digit cursor, up/down
 * step the value by the g_world_menu_number_entry_digit_steps step of that digit (clamped to the range),
 * and a changed value is redrawn as a signed number in the window. When the
 * cancel/confirm handlers change the 32-bit input mask, a -1 left in the value
 * restores the original; otherwise the value is stored and the range's
 * on_change callback runs. */
void world_menu_number_entry_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    world_menu_number_range_t* range;
    u8* buffer;
    s16 original;
    s32 digits;
    thread_t* thread;
    s32* parameter;
    menu_entry_t* param;
    s16 value;
    s32 i;
    s32 j;
    s32 digit;
    s32 redraw;
    u32 input;
    menu_icon_sprites_t* record;
    POLY_FT4* cursor_quad;
    POLY_FT4* shadow_quad;
    s32* stride;

    g_world_menu_list_controller_input = script_get_controller_input_pointer(0);
    /* A direct pen pointer folds both member-address adjustments and changes scheduling. */
    stride = &g_menu_text_state.pen.stride;
    thread = &g_threads[g_thread_current_id];
    parameter = &thread->function_parameter_1;
    param = (menu_entry_t*)*parameter;
    range = param->context;
    digits = text_count_decimal_digits(range->bounds.max);
    original = *menu_entry_result(param);
    value = original;
    if (value < range->bounds.min || range->bounds.max < value) {
        value = range->bounds.min;
    }
    world_menu_build_icon_record(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(records[1]));
    for (j = 0; j < 2; j++) {
        menu_init_default_poly_ft4(&cursor_quads[j]);
        SetSemiTrans(&cursor_quads[j], 0);
        cursor_quads[j].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        cursor_quads[j].clut = 0x7D7C;
        cursor_quads[j].u0 = 0xB7;
        cursor_quads[j].u1 = 0xB7;
        cursor_quads[j].u2 = 0xA7;
        cursor_quads[j].u3 = 0xA7;
        cursor_quads[j].v0 = 0;
        cursor_quads[j].v1 = 0x10;
        cursor_quads[j].v2 = 0;
        cursor_quads[j].v3 = 0x10;
        menu_init_default_poly_ft4(&shadow_quads[j]);
        SetSemiTrans(&shadow_quads[j], 1);
        shadow_quads[j].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        shadow_quads[j].clut = 0x7DBC;
        shadow_quads[j].u0 = 0xC7;
        shadow_quads[j].u1 = 0xC7;
        shadow_quads[j].u2 = 0xB7;
        shadow_quads[j].u3 = 0xB7;
        shadow_quads[j].v0 = 0;
        shadow_quads[j].v1 = 0x10;
        shadow_quads[j].v2 = 0;
        shadow_quads[j].v3 = 0x10;
    }
    digit = 0;
    buffer = world_menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    *stride = param->inner_width;
    world_menu_set_text_origin(8, 9);
    world_menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
    world_gfx_draw_signed_number(value, digits, buffer, (glyph_blit_t*)((u8*)stride - offsetof(glyph_blit_t, stride)));
    LoadImage(&rect, buffer);
    redraw = 1;
    for (i = 0;; i++) {
        record = &records[i & 1].base;
        *menu_entry_result(param) = value;
        thread_wait_frames(1);
        if (redraw == 1) {
            redraw = 0;
            menu_free_memory(buffer);
        }
        if (world_menu_check_thread_completion(g_world_menu_list_controller_input) != 0) {
            break;
        }
        input = *g_world_menu_list_controller_input;
        world_menu_cancel_thread_group(param);
        world_menu_handle_entry_confirm(param, 0);
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
            buffer = world_menu_build_and_upload_window_frame_image(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            *stride = param->inner_width;
            world_menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
            world_gfx_draw_signed_number(
                value, digits, buffer, (glyph_blit_t*)((u8*)stride - offsetof(glyph_blit_t, stride)));
            LoadImage(&rect, buffer);
        }
        cursor_quad = &cursor_quads[i & 1];
        shadow_quad = &shadow_quads[i & 1];
        world_menu_update_entry_cursor_quads(param, cursor_quad, shadow_quad, i, digit);
        menu_select_sprite_clut(cursor_quad);
        menu_select_sprite_clut(shadow_quad);
        if (world_thread_is_previous_running() == 0) {
            cursor_quad->clut = 0x7D7C;
            shadow_quad->clut = 0x7DBC;
        } else {
            cursor_quad->clut = 0x7DFC;
            shadow_quad->clut = 0x7E3C;
        }
        world_menu_update_icon_cursor_sprites(param, record, i, -1);
        world_menu_select_icon_cluts(record);
        gfx_draw_or_append_gpu_primitive(&cursor_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&shadow_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&record->sprites[0]);
        gfx_draw_or_append_gpu_primitive(record);
    }
    thread_yield();
    thread_exit_current();
}
