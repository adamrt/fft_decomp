#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* First thread parameter of world_menu_labeled_number_entry_thread: the numeric-entry parameter
 * of world_menu_number_entry_thread plus the label text drawn left of the number. */
typedef struct world_menu_labeled_number_param {
    s16 x;      /* 0x00 */
    s16 y;      /* 0x02 */
    s16 width;  /* 0x04 */
    s16 height; /* 0x06 */
    u8 unknown_08[0x1c - 0x08];
    s16 text_id; /* 0x1c: passed to world_text_find_entry */
    u8 unknown_1e[0x30 - 0x1e];
    world_menu_number_range_t* range; /* 0x30 */
    s16* value;                       /* 0x34 */
} world_menu_labeled_number_param_t;

/*
 * Labelled numeric entry thread: world_menu_number_entry_thread with a text label.
 *
 * Draws param->text_id left of the number and edits the halfword at
 * param->value one decimal digit at a time within param->range. The value is
 * written back each frame only while g_world_formation_screen_active is set. The initial label is
 * measured in pixels and each redraw in character columns (times 6).
 * i is shared by the quad setup loop and the frame loop, as in the target.
 */
void world_menu_labeled_number_entry_thread(void) {
    RECT rect;
    world_menu_icon_record_t record;
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    s16 label_size[2];
    world_menu_number_range_t* range;
    void* buffer;
    s16 original;
    s32 digits;
    world_thread_t* thread;
    world_menu_labeled_number_param_t* param;
    s16 value;
    s32 i;
    s32 digit;
    s32 redraw;
    u32 input;
    POLY_FT4* cursor_quad;
    POLY_FT4* shadow_quad;

    g_world_menu_list_controller_input = world_input_get_menu_controller(0);
    thread = &g_world_threads[g_world_thread_current_id];
    param = *(world_menu_labeled_number_param_t**)thread;
    range = param->range;
    digits = world_text_count_decimal_digits(range->max);
    original = *param->value;
    value = original;
    if (value < range->min || range->max < value) {
        value = range->min;
    }
    world_menu_build_icon_record(&rect, (world_menu_icon_thread_param_t*)param, &record);
    for (i = 0; i < 2; i++) {
        world_menu_init_quad(&cursor_quads[i]);
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
        world_menu_init_quad(&shadow_quads[i]);
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
    buffer = world_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
    g_world_menu_text_state.stride = param->width;
    world_text_measure_pixels(&label_size[0], &label_size[1], world_text_find_entry(param->text_id));
    world_menu_set_text_origin(8, 9);
    world_menu_display_text_entry(param->text_id, buffer, &g_world_menu_text_state.origin_x);
    /* The target passes both coordinates without the s16 sign extension. */
    ((void (*)(s32, s32))world_menu_set_text_origin)((u16)label_size[0] + 8, 9);
    ((void (*)(s32, s32))world_menu_set_text_origin)(param->width - digits * 6 - 0x10, 9);
    /* The target passes digits without the definition's u8 narrowing. */
    ((void (*)(s32, s32, void*, u16*))world_gfx_draw_signed_number)(
        value, digits, buffer, (u16*)&g_world_menu_text_state.origin_x);
    LoadImage(&rect, buffer);
    redraw = 1;
    for (i = 0;; i++) {
        if (g_world_formation_screen_active != 0) {
            *param->value = value;
        }
        world_thread_wait_frames(1);
        if (redraw == 1) {
            world_menu_free_memory(buffer);
            redraw = 0;
        }
        if (world_menu_check_thread_completion((s32*)g_world_menu_list_controller_input) != 0) {
            break;
        }
        input = *g_world_menu_list_controller_input;
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        world_menu_handle_entry_confirm((world_menu_entry_t*)param, 0);
        if (input != *g_world_menu_list_controller_input) {
            if (*param->value == -1) {
                *param->value = original;
            } else {
                *param->value = value;
                param->range->on_change();
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
                if (value <= range->max - g_world_menu_number_entry_digit_steps[digit]) {
                    value += g_world_menu_number_entry_digit_steps[digit];
                } else {
                    value = range->max;
                }
                redraw = 1;
            }
            if (*g_world_menu_list_controller_input & PSX_PAD_DOWN) {
                if (value >= range->min + g_world_menu_number_entry_digit_steps[digit]) {
                    value -= g_world_menu_number_entry_digit_steps[digit];
                } else {
                    value = range->min;
                }
                redraw = 1;
            }
        }
        if (redraw == 1) {
            buffer = world_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
            g_world_menu_text_state.stride = param->width;
            world_text_measure(&label_size[0], &label_size[1], world_text_find_entry(param->text_id));
            world_menu_set_text_origin(8, 9);
            world_menu_display_text_entry(param->text_id, buffer, &g_world_menu_text_state.origin_x);
            /* The target passes both coordinates without the s16 sign extension. */
            ((void (*)(s32, s32))world_menu_set_text_origin)((u16)label_size[0] * 6 + 8, 9);
            ((void (*)(s32, s32))world_menu_set_text_origin)(param->width - digits * 6 - 0x10, 9);
            /* The target passes digits without the definition's u8 narrowing. */
            ((void (*)(s32, s32, void*, u16*))world_gfx_draw_signed_number)(
                value, digits, buffer, (u16*)&g_world_menu_text_state.origin_x);
            LoadImage(&rect, buffer);
        }
        cursor_quad = &cursor_quads[i & 1];
        shadow_quad = &shadow_quads[i & 1];
        world_menu_update_entry_cursor_quads((world_menu_entry_t*)param, cursor_quad, shadow_quad, i, digit);
        world_menu_select_task_icon_tile((world_texture_prim_t*)cursor_quad);
        world_menu_select_task_icon_tile((world_texture_prim_t*)shadow_quad);
        if (world_thread_is_previous_running() == 0) {
            cursor_quad->clut = 0x7D7C;
            shadow_quad->clut = 0x7DBC;
        } else {
            cursor_quad->clut = 0x7DFC;
            shadow_quad->clut = 0x7E3C;
        }
        world_menu_update_icon_cursor_sprites((world_menu_icon_thread_param_t*)param, &record.base, i, -1);
        world_menu_select_icon_cluts(&record.base);
        world_gfx_draw_or_append_gpu_primitive(&cursor_quads[i & 1]);
        world_gfx_draw_or_append_gpu_primitive(&shadow_quads[i & 1]);
        world_gfx_draw_or_append_gpu_primitive(&record.base.sprites[0]);
        world_gfx_draw_or_append_gpu_primitive(&record);
    }
    world_thread_yield();
    world_thread_exit_current();
}
