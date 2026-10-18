/* The item detail panel's status column of immunity and innate or starting status icons. */

#include "fft/bits.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/status.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Status image rectangles, indexed by status id, that item_draw_status_list draws in the item detail panel's status
 * column. */
extern RECT g_unit_panel_status_display_image_rects[];

/* Draw the item detail panel's status column: immunity icons, then the
 * innate/starting status icons.
 *
 * The first two passes count the set bits (clamped to 12 rows) so the column
 * can be bottom-aligned; the last two emit one backing quad plus one status
 * image quad per status. */
void item_draw_status_list(s16 item_id) {
    RECT rect;
    u16 icon_tpage;
    u16 tpage;
    s16 count = 0;
    s32 attribute = g_item_primary_data[item_id].attributes;
    s32 drawn;
    s32 bit;
    s32 i;
    s32 x;
    s32 y;

    bits_init_primary_reader(g_item_attributes[attribute].immunity);
    for (i = 0; i < STATUS_COUNT; i++) {
        if (bits_read_primary(1) != 0) {
            count++;
        }
    }
    bits_init_primary_reader(g_item_attributes[attribute].innate);
    bits_init_secondary_reader(g_item_attributes[attribute].starting);
    for (i = 0; i < STATUS_COUNT; i++) {
        bit = bits_read_primary(1);
        bit |= bits_read_secondary(1);
        if (bit != 0) {
            count++;
        }
    }
    if (count >= 13) {
        count = 12;
    }
    bit = 0xC2;
    if (g_menu_selected_slot == MENU_EQUIPMENT_SLOT_BODY) {
        bit = 0xDA;
    }
    y = bit - count * 9;
    x = 0xD1;
    tpage = GetTPage(0, 0, 0x180, 0);
    drawn = 0;
    icon_tpage = g_menu_cursor_texture_page;
    bits_init_primary_reader(g_item_attributes[attribute].immunity);
    for (i = 0; i < STATUS_COUNT; i++) {
        if (drawn >= count) {
            break;
        }
        if (bits_read_primary(1) != 0) {
            rect.x = x - 7;
            rect.y = y + 1;
            rect.w = 8;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0xA9, 0xF5, 0, 0, tpage, 0x3FD9, 8);
            rect.x = x;
            rect.y = y;
            rect.w = g_unit_panel_status_display_image_rects[i].w;
            rect.h = g_unit_panel_status_display_image_rects[i].h;
            menu_gfx_enqueue_textured_quad(&rect, g_unit_panel_status_display_image_rects[i].x,
                g_unit_panel_status_display_image_rects[i].y, 0, 0, icon_tpage, 0x3FD9, 8);
            y += 9;
            drawn++;
        }
    }
    bits_init_primary_reader(g_item_attributes[attribute].innate);
    bits_init_secondary_reader(g_item_attributes[attribute].starting);
    for (i = 0; i < STATUS_COUNT; i++) {
        if (drawn >= count) {
            break;
        }
        bit = bits_read_primary(1);
        bit |= bits_read_secondary(1);
        if (bit != 0) {
            rect.x = x - 8;
            rect.y = y + 1;
            rect.w = 8;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0xA0, 0xF5, 0, 0, tpage, 0x3FD8, 8);
            rect.x = x;
            rect.y = y;
            rect.w = g_unit_panel_status_display_image_rects[i].w;
            rect.h = g_unit_panel_status_display_image_rects[i].h;
            menu_gfx_enqueue_textured_quad(&rect, g_unit_panel_status_display_image_rects[i].x,
                g_unit_panel_status_display_image_rects[i].y, 0, 0, icon_tpage, 0x3FD8, 8);
            y += 9;
            drawn++;
        }
    }
}
