#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Menu-list layout record in the WORLD menu script stream. */
typedef struct world_menu_list_record {
    u8 type;   /* 0x00: 0x10 = list layout, 0x1c = end of stream */
    u8 length; /* 0x01: byte length of this record */
    u8 unknown_02[2];
    u8 x;            /* 0x04 */
    u8 row_height;   /* 0x05 */
    u8 visible_rows; /* 0x06 */
    u8 unknown_07[2];
    u8 width_a; /* 0x09 */
    u8 width_b; /* 0x0a */
} world_menu_list_record_t;

/* Text image upload: the VRAM rectangle followed by a -1 terminated list of
 * text ids for world_text_render_id_list_to_image_rows. The list length is inferred from the 0x80-byte
 * frame (at -O1 the frame carries 56 further bytes of spill slots). */
typedef struct {
    RECT rect;
    s16 text_ids[20];
} world_menu_list_image_t;

/* Find the next list-layout record in a menu script stream, apply its
 * geometry, clamp the cursor and scroll position to the option count, and
 * redraw the visible text ids when a list font is active. */
void world_menu_load_scrollable_list_layout(world_menu_list_record_t* record) {
    world_menu_list_image_t image;
    s32 i;

    while (record->type != 0x10) {
        if (record->type == 0x1C) {
            return;
        }
        record = (world_menu_list_record_t*)((u8*)record + record->length);
    }
    g_world_menu_list_row_record_count = record->x;
    g_world_menu_row_height = record->row_height;
    g_world_menu_list_visible_rows = record->visible_rows;
    g_world_menu_list_glyph_width = record->width_a;
    g_world_menu_list_text_columns = record->width_b;
    g_world_menu_list_image_width = (g_world_menu_list_glyph_width * g_world_menu_list_text_columns) >> 2;
    if (g_world_menu_cursor_position >= g_world_menu_option_count) {
        g_world_menu_cursor_position = g_world_menu_option_count - 1;
    }
    if (g_world_menu_cursor_position - g_world_menu_scroll_offset >= g_world_menu_list_visible_rows) {
        g_world_menu_scroll_offset = g_world_menu_cursor_position;
    }
    if (g_world_menu_cursor_position < g_world_menu_scroll_offset) {
        g_world_menu_scroll_offset = g_world_menu_cursor_position;
    }
    if (g_world_menu_option_count < g_world_menu_list_visible_rows) {
        g_world_menu_scroll_offset = 0;
    } else if (g_world_menu_option_count - g_world_menu_scroll_offset < g_world_menu_list_visible_rows) {
        g_world_menu_scroll_offset = g_world_menu_option_count - g_world_menu_list_visible_rows;
    }
    if (g_world_menu_cursor_position >= g_world_menu_option_count) {
        g_world_menu_cursor_position = g_world_menu_option_count - 1;
    }
    if (g_world_menu_list_text_table != 0) {
        image.rect.x = 0x240;
        image.rect.y = 0x130;
        image.rect.w = g_world_menu_list_image_width;
        image.rect.h = g_world_menu_list_visible_rows * 16;
        ClearImage(&image.rect, 0, 0, 0);
        for (i = 0; i < g_world_menu_list_visible_rows; i++) {
            image.text_ids[i] = g_world_menu_list_entry_ids[i + g_world_menu_scroll_offset];
        }
        image.text_ids[i] = -1;
        image.rect.x = 0x240;
        image.rect.y = 0x130;
        image.rect.w = g_world_menu_list_image_width;
        image.rect.h = g_world_menu_row_height;
        world_text_render_id_list_to_image_rows((u8*)g_world_menu_list_text_table, image.text_ids, &image.rect, 0);
    }
    g_world_menu_list_scroll_direction = 0;
    g_world_menu_list_layout_pending = 0;
}
