#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Menu-script list record (command 0x10). */
typedef struct world_menu_list_draw_record {
    u8 type;            /* 0x00 */
    u8 length;          /* 0x01 */
    u8 show_cursor;     /* 0x02 */
    u8 _unused_03;      /* 0x03 */
    u8 skip_count;      /* 0x04 */
    u8 _unused_05[2];   /* 0x05 */
    u8 x;               /* 0x07 */
    u8 y;               /* 0x08 */
    u8 _unused_09[2];   /* 0x09 */
    u8 cursor_x_offset; /* 0x0b */
    u8 arrow_up_y;      /* 0x0c */
    u8 arrow_down_y;    /* 0x0d */
} world_menu_list_draw_record_t;

typedef struct world_chained_record world_chained_record_t;

extern world_chained_record_t* world_skip_chained_records(world_chained_record_t* record);

u8* world_menu_script_draw_scrollable_list(world_menu_list_draw_record_t* record) {
    RECT rect;
    s32 unused[4];
    world_menu_point_t point;
    u16 priority;
    s32 speed;
    s32 i;
    s32 j;
    s32 rows;
    s32 index;
    u16 old_cursor;
    u8* base;

    priority = g_world_menu_draw_priority;
    if (g_world_menu_option_count <= 0) {
        for (j = record->skip_count; j != -1; j--) {
            record = (world_menu_list_draw_record_t*)world_skip_chained_records((world_chained_record_t*)record);
        }
        return (u8*)record;
    }
    speed = world_gfx_get_vsync_mode_or_one();
    g_world_menu_use_scroll_position = 1;
    if (*(s8*)&g_world_menu_list_layout_pending != 0) {
        world_menu_load_scrollable_list_layout((world_menu_list_record_t*)record);
    }
    old_cursor = g_world_menu_cursor_position;
    if (*(s8*)&g_world_menu_list_scroll_direction == 0) {
        i = world_input_read_menu_scroll_repeat();
        if (i != 0) {
            if (i != 2) {
                world_menu_scroll_list_page(i, (u8*)record);
            }
        } else if ((g_world_menu_display_script_input & 0x10000000)
            || (g_world_menu_display_script_input & PSX_PAD_UP)) {
            index = g_world_menu_scroll_offset;
            if (index == g_world_menu_cursor_position) {
                if (index > 0) {
                    *(s8*)&g_world_menu_list_scroll_direction = -1;
                    if (g_world_menu_list_text_table != 0) {
                        g_world_menu_list_row_text_ids[0] = g_world_menu_list_entry_ids[index - 1];
                        rect.x = 0x240;
                        rect.y = 0x120;
                        rect.w = g_world_menu_list_image_width;
                        rect.h = g_world_menu_row_height;
                        world_text_render_id_list_to_image_rows(
                            (u8*)g_world_menu_list_text_table, g_world_menu_list_row_text_ids, &rect, 0);
                    }
                }
            } else if (g_world_menu_display_script_input & PSX_PAD_UP) {
                g_world_menu_cursor_position--;
            }
        } else if ((g_world_menu_display_script_input & 0x40000000)
            || (g_world_menu_display_script_input & PSX_PAD_DOWN)) {
            if (g_world_menu_scroll_offset + g_world_menu_list_visible_rows - 1 == g_world_menu_cursor_position) {
                if (g_world_menu_scroll_offset + g_world_menu_list_visible_rows - 1 < g_world_menu_option_count - 1) {
                    g_world_menu_list_scroll_direction = 1;
                    if (g_world_menu_list_text_table != 0) {
                        index = g_world_menu_scroll_offset + g_world_menu_list_visible_rows - 1;
                        g_world_menu_list_row_text_ids[0] = g_world_menu_list_entry_ids[index + 1];
                        rect.x = 0x240;
                        rect.y = g_world_menu_list_visible_rows * (s16)g_world_menu_row_height + 0x130;
                        rect.w = g_world_menu_list_image_width;
                        rect.h = g_world_menu_row_height;
                        world_text_render_id_list_to_image_rows(
                            (u8*)g_world_menu_list_text_table, g_world_menu_list_row_text_ids, &rect, 0);
                    }
                }
            } else {
                if (g_world_menu_cursor_position < g_world_menu_option_count - 1
                    && (g_world_menu_display_script_input & PSX_PAD_DOWN)) {
                    g_world_menu_cursor_position++;
                }
            }
        } else {
            g_world_menu_scroll_repeat_timer = 0;
        }
    }
    if (*(s8*)&g_world_menu_list_scroll_direction < 0) {
        if (g_world_menu_scroll_repeat_timer >= g_main_menu_scroll_accel_delay) {
            g_world_menu_scroll_pixel_offset -= g_main_menu_scroll_fast_step * (u8)speed;
        } else {
            g_world_menu_scroll_repeat_timer += (u8)speed;
            g_world_menu_scroll_pixel_offset -= g_main_menu_scroll_slow_step * (u8)speed;
        }
        if (g_world_menu_scroll_pixel_offset <= -(s16)g_world_menu_row_height) {
            g_world_menu_scroll_pixel_offset = 0;
            g_world_menu_list_scroll_direction = 0;
            g_world_menu_scroll_offset = --g_world_menu_cursor_position;
            /* Both text-table checks skip straight to the arrows, past the
               table test below (the target does not re-test it). */
            if (g_world_menu_list_text_table == 0) {
                goto arrows;
            }
            for (i = 0; i < g_world_menu_list_visible_rows; i++) {
                rect.x = 0x240;
                rect.y = (i - 1) * (s16)g_world_menu_row_height + 0x130;
                rect.w = g_world_menu_list_image_width;
                rect.h = g_world_menu_row_height;
                world_gfx_add_draw_move_primitive(&rect, 0x240, (s16)g_world_menu_row_height * i + 0x130, priority - 1);
            }
        }
    } else if (*(s8*)&g_world_menu_list_scroll_direction > 0) {
        if (g_world_menu_scroll_repeat_timer >= g_main_menu_scroll_accel_delay) {
            g_world_menu_scroll_pixel_offset += g_main_menu_scroll_fast_step * (u8)speed;
        } else {
            g_world_menu_scroll_repeat_timer += (u8)speed;
            g_world_menu_scroll_pixel_offset += g_main_menu_scroll_slow_step * (u8)speed;
        }
        if (g_world_menu_scroll_pixel_offset >= (s16)g_world_menu_row_height) {
            g_world_menu_scroll_pixel_offset = 0;
            g_world_menu_list_scroll_direction = 0;
            g_world_menu_scroll_offset++;
            g_world_menu_cursor_position++;
            if (g_world_menu_list_text_table == 0) {
                goto arrows;
            }
            rect.x = 0x240;
            rect.y = (s16)g_world_menu_row_height + 0x130;
            rect.w = g_world_menu_list_image_width;
            rect.h = (s16)g_world_menu_row_height * g_world_menu_list_visible_rows;
            world_gfx_add_draw_move_primitive(&rect, 0x240, 0x130, priority - 1);
        }
    }
    if (g_world_menu_list_text_table != 0) {
        if (g_world_menu_scroll_pixel_offset != 0) {
            rect.x = record->x;
            rect.y = record->y - 5;
            rect.w = g_world_menu_list_image_width * 4;
            rect.h = g_world_menu_row_height * (u16)g_world_menu_list_visible_rows + 10;
            world_gfx_enqueue_textured_quad(&rect, 0, g_world_menu_scroll_pixel_offset + 0x2B, 0,
                g_world_menu_semi_trans, g_world_menu_window_tpage, g_world_menu_window_clut,
                g_world_menu_draw_priority);
        } else {
            rect.x = record->x;
            rect.y = record->y;
            rect.w = g_world_menu_list_image_width * 4;
            rect.h = g_world_menu_row_height * (u16)g_world_menu_list_visible_rows;
            world_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_world_menu_semi_trans, g_world_menu_window_tpage,
                g_world_menu_window_clut, g_world_menu_draw_priority);
        }
    }
arrows:
    if (g_world_menu_use_alternate_palette == 0) {
        if (g_world_menu_scroll_offset != 0) {
            rect.x = g_world_menu_window_right_x;
            rect.y = record->arrow_up_y;
            rect.w = 8;
            rect.h = 0x10;
            world_gfx_enqueue_textured_quad(&rect, 0xD8, 0, 0, g_world_menu_semi_trans, g_world_menu_icon_tpage,
                g_world_menu_clut_front, g_world_menu_draw_priority);
        }
        if (g_world_menu_scroll_offset < g_world_menu_option_count - g_world_menu_list_visible_rows) {
            rect.x = g_world_menu_window_right_x;
            rect.y = record->arrow_down_y;
            rect.w = 8;
            rect.h = 0x10;
            world_gfx_enqueue_textured_quad(&rect, 0xE0, 0, 0, g_world_menu_semi_trans, g_world_menu_icon_tpage,
                g_world_menu_clut_front, g_world_menu_draw_priority);
        }
        if (g_world_menu_option_count > g_world_menu_list_visible_rows) {
            i = record->arrow_up_y + 0xF;
            j = record->arrow_down_y - (s16)(i + 5);
            i += j * g_world_menu_cursor_position / (g_world_menu_option_count - 1);
            rect.x = g_world_menu_window_right_x;
            rect.y = i;
            rect.w = 8;
            rect.h = 8;
            world_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_world_menu_semi_trans, g_world_menu_icon_tpage,
                g_world_menu_clut_front, g_world_menu_draw_priority);
        }
    }
    if (record->show_cursor != 0) {
        point.x = record->x - record->cursor_x_offset;
        point.y
            = record->y + (g_world_menu_cursor_position - g_world_menu_scroll_offset) * (s16)g_world_menu_row_height;
        world_menu_draw_animated_cursor(&point, &g_world_menu_list_cursor_anim, g_world_menu_use_alternate_palette);
    }
    record = (world_menu_list_draw_record_t*)((u8*)record + record->length);
    g_world_menu_scroll_row_offset = 0;
    rows = g_world_menu_list_visible_rows + 1;
    if (g_world_menu_scroll_pixel_offset == 0) {
        rows--;
    }
    base = (u8*)record;
    for (i = 0; i < rows; i++, g_world_menu_scroll_row_offset++) {
        if (g_world_menu_scroll_offset + i > g_world_menu_option_count - 1 && g_world_menu_scroll_pixel_offset == 0) {
            break;
        }
        record = (world_menu_list_draw_record_t*)base;
        for (j = 0; j < g_world_menu_list_row_record_count; j++) {
            record = (world_menu_list_draw_record_t*)g_world_menu_script_handlers[record->type]((u8*)record);
        }
    }
    g_world_menu_use_scroll_position = 0;
    if (old_cursor != g_world_menu_cursor_position) {
        g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return (u8*)record;
}
