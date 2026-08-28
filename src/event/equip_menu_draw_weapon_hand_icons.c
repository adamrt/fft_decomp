#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Draw the handling icons for `weapon_id` (ids 1-0x79) in the item detail
 * panel from its weapon secondary flags. */
void equip_menu_draw_weapon_hand_icons(s16 weapon_id) {
    RECT rect;
    s16 x;
    s16 y;
    u8 flags;
    s32 forced;
    s32 two_hands;
    s32 two_swords;

    if (weapon_id != 0 && weapon_id < 0x7A) {
        flags = g_main_item_weapon_data[weapon_id].flags;
        forced = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
        two_hands = flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE;
        two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        y = 0xD1;
        x = 0xC9;
        if (forced) {
            rect.x = x;
            rect.y = y;
            rect.w = 8;
            rect.h = 8;
            equip_gfx_enqueue_textured_quad(&rect, 0xB8, 0x34, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 0xD2;
            rect.w = 0x15;
            equip_gfx_enqueue_textured_quad(&rect, 0xBE, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 0xE8;
            rect.w = 0x10;
            rect.y = y;
            rect.h = 0xA;
            equip_gfx_enqueue_textured_quad(&rect, 0xC9, 0x6E, 0, 0, 0x1F, 0x7D7C, 8);
            return;
        }
        if (two_hands) {
            rect.x = x;
            rect.y = y;
            rect.w = 0x13;
            rect.h = 8;
            equip_gfx_enqueue_textured_quad(&rect, 0, 0xD0, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 0xDD;
            rect.w = 0x15;
            equip_gfx_enqueue_textured_quad(&rect, 0xBE, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
            y = 0xC5;
        }
        if (two_swords) {
            rect.x = x;
            rect.y = y;
            rect.w = 8;
            rect.h = 8;
            equip_gfx_enqueue_textured_quad(&rect, 0xB8, 0x34, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 0xD2;
            rect.y = y;
            rect.w = 0x18;
            rect.h = 8;
            equip_gfx_enqueue_textured_quad(&rect, 0, 0xC8, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 0xEA;
            rect.w = 4;
            equip_gfx_enqueue_textured_quad(&rect, 0xCF, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
        }
        if (!two_hands && !two_swords) {
            rect.x = x;
            rect.y = y;
            rect.w = 6;
            rect.h = 8;
            equip_gfx_enqueue_textured_quad(&rect, 0xB1, 0x34, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 0xD0;
            rect.w = 0x11;
            equip_gfx_enqueue_textured_quad(&rect, 0xBE, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 0xE2;
            rect.w = 0x10;
            rect.y = y;
            rect.h = 0xA;
            equip_gfx_enqueue_textured_quad(&rect, 0xC9, 0x6E, 0, 0, 0x1F, 0x7D7C, 8);
        }
    }
}
