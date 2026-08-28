#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/status.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

extern item_data_t g_main_item_primary_data[];

/* Flat view of item_attribute_t. The target addresses `starting` as
 * attribute * 25 + (base + 15), rebuilt from the live base + 5 register;
 * the nested status_sets member instead reuses the innate address + 10. */
typedef struct item_attribute_flat {
    u8 attributes[3];
    u8 move;
    u8 jump;
    u8 innate[5];
    u8 immunity[5];
    u8 starting[5];
    u8 elemental_affinity[5];
} item_attribute_flat_t;

#define ITEM_ATTRIBUTES_FLAT ((item_attribute_flat_t*)g_main_item_attributes)

/* Draw the item detail panel's status column: immunity icons, then the
 * innate/starting status icons.
 *
 * The first two passes count the set bits (clamped to 12 rows) so the column
 * can be bottom-aligned; the last two emit one backing quad plus one status
 * image quad per status. */
void world_item_build_status_list_polygons(s16 item_id) {
    RECT rect;
    u16 icon_tpage;
    u16 tpage;
    s16 count;
    s32 attribute;
    s32 drawn;
    s32 bit;
    s32 i;
    s32 x;
    s32 y;

    count = 0;
    attribute = g_main_item_primary_data[item_id].attributes;
    world_bit_cursor_set_primary(ITEM_ATTRIBUTES_FLAT[attribute].immunity);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        if (world_bit_cursor_read_primary(1) != 0) {
            count++;
        }
    }
    world_bit_cursor_set_primary(ITEM_ATTRIBUTES_FLAT[attribute].innate);
    world_bit_cursor_set_secondary(ITEM_ATTRIBUTES_FLAT[attribute].starting);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        bit = world_bit_cursor_read_primary(1);
        bit |= world_bit_cursor_read_secondary(1);
        if (bit != 0) {
            count++;
        }
    }
    if (count >= 13) {
        count = 12;
    }
    bit = 0xC2;
    if (g_world_item_menu_category == 3) {
        bit = 0xDA;
    }
    y = bit - count * 9;
    x = 0xD1;
    tpage = GetTPage(0, 0, 0x180, 0);
    drawn = 0;
    icon_tpage = g_world_menu_icon_tpage;
    world_bit_cursor_set_primary(ITEM_ATTRIBUTES_FLAT[attribute].immunity);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        if (drawn >= count) {
            break;
        }
        if (world_bit_cursor_read_primary(1) != 0) {
            rect.x = x - 7;
            rect.y = y + 1;
            rect.w = 8;
            rect.h = 8;
            world_gfx_enqueue_textured_quad(&rect, 0xA9, 0xF5, 0, 0, tpage, 0x3FD9, 8);
            rect.x = x;
            rect.y = y;
            rect.w = g_world_status_display_image_rects[i].w;
            rect.h = g_world_status_display_image_rects[i].h;
            world_gfx_enqueue_textured_quad(&rect, g_world_status_display_image_rects[i].x,
                g_world_status_display_image_rects[i].y, 0, 0, icon_tpage, 0x3FD9, 8);
            y += 9;
            drawn++;
        }
    }
    world_bit_cursor_set_primary(ITEM_ATTRIBUTES_FLAT[attribute].innate);
    world_bit_cursor_set_secondary(ITEM_ATTRIBUTES_FLAT[attribute].starting);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        if (drawn >= count) {
            break;
        }
        bit = world_bit_cursor_read_primary(1);
        bit |= world_bit_cursor_read_secondary(1);
        if (bit != 0) {
            rect.x = x - 8;
            rect.y = y + 1;
            rect.w = 8;
            rect.h = 8;
            world_gfx_enqueue_textured_quad(&rect, 0xA0, 0xF5, 0, 0, tpage, 0x3FD8, 8);
            rect.x = x;
            rect.y = y;
            rect.w = g_world_status_display_image_rects[i].w;
            rect.h = g_world_status_display_image_rects[i].h;
            world_gfx_enqueue_textured_quad(&rect, g_world_status_display_image_rects[i].x,
                g_world_status_display_image_rects[i].y, 0, 0, icon_tpage, 0x3FD8, 8);
            y += 9;
            drawn++;
        }
    }
}
