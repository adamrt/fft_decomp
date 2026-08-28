#include "fft/data.h"
#include "fft/equip.h"
#include "fft/main_runtime.h"
#include "fft/status.h"
#include "psx/gpu.h"
#include "psx/types.h"

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

/* The callee ignores the trailing priority argument the caller still passes. */
#define EQUIP_QUAD                                                                                                     \
    ((void (*)(const RECT*, s32, s32, const u8*, s32, u16, u16, s32))equip_gfx_enqueue_textured_quad_current_ot)

#define ITEM_ATTRIBUTES_FLAT ((item_attribute_flat_t*)g_main_item_attributes)

/* Draw the item detail panel's status column: immunity icons, then the
 * innate/starting status icons.
 *
 * The first two passes count the set bits (clamped to 12 rows) so the column
 * can be bottom-aligned; the last two emit one backing quad plus one status
 * image quad per status. */
void equip_menu_draw_item_status_list(s16 item_id) {
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
    equip_bits_init_primary_reader(ITEM_ATTRIBUTES_FLAT[attribute].immunity);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        if (equip_bits_read_primary(1) != 0) {
            count++;
        }
    }
    equip_bits_init_primary_reader(ITEM_ATTRIBUTES_FLAT[attribute].innate);
    equip_bits_init_secondary_reader(ITEM_ATTRIBUTES_FLAT[attribute].starting);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        bit = equip_bits_read_primary(1);
        bit |= equip_bits_read_secondary(1);
        if (bit != 0) {
            count++;
        }
    }
    if (count >= 13) {
        count = 12;
    }
    bit = 0xC2;
    if (g_equip_selected_slot == 3) {
        bit = 0xDA;
    }
    y = bit - count * 9;
    x = 0xD1;
    tpage = GetTPage(0, 0, 0x180, 0);
    drawn = 0;
    icon_tpage = g_equip_menu_cursor_texture_page;
    equip_bits_init_primary_reader(ITEM_ATTRIBUTES_FLAT[attribute].immunity);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        if (drawn >= count) {
            break;
        }
        if (equip_bits_read_primary(1) != 0) {
            rect.x = x - 7;
            rect.y = y + 1;
            rect.w = 8;
            rect.h = 8;
            EQUIP_QUAD(&rect, 0xA9, 0xF5, 0, 0, tpage, 0x3FD9, 8);
            rect.x = x;
            rect.y = y;
            rect.w = g_equip_status_display_image_rects[i].w;
            rect.h = g_equip_status_display_image_rects[i].h;
            EQUIP_QUAD(&rect, g_equip_status_display_image_rects[i].x, g_equip_status_display_image_rects[i].y, 0, 0,
                icon_tpage, 0x3FD9, 8);
            y += 9;
            drawn++;
        }
    }
    equip_bits_init_primary_reader(ITEM_ATTRIBUTES_FLAT[attribute].innate);
    equip_bits_init_secondary_reader(ITEM_ATTRIBUTES_FLAT[attribute].starting);
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        if (drawn >= count) {
            break;
        }
        bit = equip_bits_read_primary(1);
        bit |= equip_bits_read_secondary(1);
        if (bit != 0) {
            rect.x = x - 8;
            rect.y = y + 1;
            rect.w = 8;
            rect.h = 8;
            EQUIP_QUAD(&rect, 0xA0, 0xF5, 0, 0, tpage, 0x3FD8, 8);
            rect.x = x;
            rect.y = y;
            rect.w = g_equip_status_display_image_rects[i].w;
            rect.h = g_equip_status_display_image_rects[i].h;
            EQUIP_QUAD(&rect, g_equip_status_display_image_rects[i].x, g_equip_status_display_image_rects[i].y, 0, 0,
                icon_tpage, 0x3FD8, 8);
            y += 9;
            drawn++;
        }
    }
}
