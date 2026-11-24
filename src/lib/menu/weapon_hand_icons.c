/* The item panel's marks for the hands a weapon needs or allows. */

#include "fft/game/item.h"
#include "fft/menu/menu_gfx.h"
#include "fft/party/item_list.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw a weapon's hand marks on the item panel: a weapon that needs both hands shows only that
 * mark; otherwise the Two Hands and Two Swords marks it allows, or the one-hand mark when it
 * allows neither. Throwables and no weapon draw nothing. */
void item_list_draw_weapon_hand_icons(s16 weapon_id) {
    RECT rect;
    s16 x;
    s16 y;
    u8 flags;
    s32 forced;
    s32 two_hands;
    s32 two_swords;

    if (weapon_id != 0 && weapon_id < ITEM_ID_THROWABLE_FIRST) {
        flags = g_item_weapon_data[weapon_id].flags;
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
            menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0x34, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 210;
            rect.w = 21;
            menu_gfx_enqueue_textured_quad(&rect, 0xBE, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 232;
            rect.w = 16;
            rect.y = y;
            rect.h = 10;
            menu_gfx_enqueue_textured_quad(&rect, 0xC9, 0x6E, 0, 0, 0x1F, 0x7D7C, 8);
            return;
        }
        if (two_hands) {
            rect.x = x;
            rect.y = y;
            rect.w = 19;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0, 0xD0, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 221;
            rect.w = 21;
            menu_gfx_enqueue_textured_quad(&rect, 0xBE, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
            y = 0xC5;
        }
        if (two_swords) {
            rect.x = x;
            rect.y = y;
            rect.w = 8;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0x34, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 210;
            rect.y = y;
            rect.w = 24;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0, 0xC8, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 234;
            rect.w = 4;
            menu_gfx_enqueue_textured_quad(&rect, 0xCF, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
        }
        if (!two_hands && !two_swords) {
            rect.x = x;
            rect.y = y;
            rect.w = 6;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0xB1, 0x34, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 208;
            rect.w = 17;
            menu_gfx_enqueue_textured_quad(&rect, 0xBE, 0x58, 0, 0, 0x1F, 0x7D7C, 8);
            rect.x = 226;
            rect.w = 16;
            rect.y = y;
            rect.h = 10;
            menu_gfx_enqueue_textured_quad(&rect, 0xC9, 0x6E, 0, 0, 0x1F, 0x7D7C, 8);
        }
    }
}
