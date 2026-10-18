/* The item list a normal shop or the poachers' den offers, hiding or marking what the selected unit cannot equip. */

#include "fft/equipment.h"
#include "fft/item.h"
#include "fft/option.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/shop.h"
#include "psx/types.h"

/* Two-byte shop availability mask, stored most significant byte first: bit
 * 0x8000 >> location id is set when that town's outfitter sells the item. */
typedef struct shop_item_availability {
    u8 high;
    u8 low;
} shop_item_availability_t;

/* One big-endian town mask per item ID, including the Random/None slots. */
static shop_item_availability_t g_shop_item_availability[ITEM_ID_COUNT] = {
    { 0x00, 0x00 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0xFE, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0xFE, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0x00, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x7C, 0x4B },
    { 0x7C, 0x4B },
    { 0x7C, 0x4B },
    { 0x00, 0x01 },
    { 0x00, 0x11 },
    { 0x00, 0x11 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x7C, 0x11 },
    { 0x7C, 0x11 },
    { 0x7C, 0x11 },
    { 0x7C, 0x11 },
    { 0x7C, 0x11 },
    { 0x7C, 0x11 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x01 },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x4B },
    { 0x00, 0x4B },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xFF },
    { 0x03, 0xFF },
    { 0x03, 0xFF },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0xFC, 0x4B },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0xFC, 0x01 },
    { 0x00, 0x01 },
    { 0xFC, 0x01 },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x03, 0xEF },
    { 0x00, 0x01 },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0x00, 0x01 },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0x00, 0x01 },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0x00, 0x01 },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0x00, 0x01 },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0xFF, 0xFF },
    { 0x00, 0x00 },
    { 0x00, 0x00 },
};

/* Build the -1 terminated item list offered by a normal shop or the Fur Shop
 * (poachers' den), sorted, and return its count.
 *
 * A normal shop selects items whose big-endian availability mask contains the
 * requested shop bit and whose required progression does not exceed the
 * shop item availability script variable. Shop ids from
 * SCREEN_CONTEXT_FUR_SHOP (100) up select the poached items in stock.
 * Entries the selected unit cannot equip are hidden or marked with 0x4000,
 * according to mode and the Show Unequippable Items option.
 *
 * Three constructs are load-bearing:
 *   - The tied launder on primary_category at the top of loop 1. The `(u16)`
 *     casts create the zero-extension the target emits twice here
 *     (`andi v1,s4,0xffff` then `andi v0,v0,0xffff`), but cse then shares one
 *     masked copy across both loops, which frees a callee-saved register and
 *     deletes the target's `sw zero,32(sp)` count spill. The launder makes
 *     primary_category opaque inside loop 1, so the mask is recomputed in the
 *     body here and stays hoisted into loop 2's preheader, as the target has
 *     it. No other spelling of loop 1's category compare (operand order,
 *     either cast form, masking the secondary too) avoids the shared copy.
 *   - The `$16` pin on `entry`, which fixes the s0/s1/s2 rotation. Pinning
 *     item_id instead does not work: it is the basic induction variable, and
 *     pinning it kills strength reduction.
 *   - `entry = item_id | 0x4000;` rather than `entry |= 0x4000;`. The target
 *     computes `ori s0,s2,0x4000` with distinct source and destination; the
 *     compound form makes the destination self-referencing (`ori s0,s0`).
 */
s32 shop_build_stock_list(s16 unit_id, s16 shop_id, s16 category, s16* entries, s32 mode) {
    s32 count;
    s32 item_id;
    register s32 entry __asm__("$16");
    s32 can_equip;
    u16 item_category;
    u16 primary_category;
    u16 secondary_category;

    if (mode == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && g_game_options.fields.show_unequippable_items != GAME_OPTION_ON) {
        mode = ITEM_LIST_MODE_EQUIPPABLE;
    }

    count = 0;
    if (category < 2) { /* either hand */
        primary_category = ITEM_MENU_CATEGORY_WEAPON;
        secondary_category = ITEM_MENU_CATEGORY_SHIELD;
    } else if (category == ITEM_LIST_FILTER_HEADGEAR) {
        primary_category = ITEM_MENU_CATEGORY_HEADGEAR;
        secondary_category = ITEM_MENU_CATEGORY_HEADGEAR;
    } else if (category == ITEM_LIST_FILTER_BODY_ARMOR) {
        primary_category = ITEM_MENU_CATEGORY_BODY_ARMOR;
        secondary_category = ITEM_MENU_CATEGORY_BODY_ARMOR;
    } else if (category == ITEM_LIST_FILTER_ACCESSORY) {
        primary_category = ITEM_MENU_CATEGORY_ACCESSORY;
        secondary_category = ITEM_MENU_CATEGORY_ACCESSORY;
    } else if (category == ITEM_LIST_FILTER_WEAPON) {
        primary_category = ITEM_MENU_CATEGORY_WEAPON;
        secondary_category = ITEM_MENU_CATEGORY_WEAPON;
    } else {
        secondary_category = ITEM_MENU_CATEGORY_OTHER;
        if (category == ITEM_LIST_FILTER_SHIELD) {
            primary_category = ITEM_MENU_CATEGORY_SHIELD;
            secondary_category = ITEM_MENU_CATEGORY_SHIELD;
        } else {
            primary_category = ITEM_MENU_CATEGORY_OTHER;
        }
    }

    if (shop_id < SCREEN_CONTEXT_FUR_SHOP) {
        s32 progression;
        s32 shop_mask;

        progression = script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY);
        item_id = 0;
        shop_mask = 0x8000 >> shop_id;
        for (; item_id < ITEM_ID_END; item_id++) {
            __asm__("" : "=r"(primary_category) : "0"(primary_category));
            item_category = item_get_menu_category(item_id);
            entry = item_id;
            if (primary_category != item_category && secondary_category != item_category) {
                continue;
            }
            if (!((g_shop_item_availability[item_id].low | (g_shop_item_availability[item_id].high << 8))
                    & shop_mask)) {
                continue;
            }
            if (progression < g_item_primary_data[item_id].shop_availability) {
                continue;
            }
            if (mode != ITEM_LIST_MODE_ALL) {
                can_equip = equipment_can_equip_item(unit_id, item_id);
                if (mode == ITEM_LIST_MODE_EQUIPPABLE && can_equip == -1) {
                    continue;
                }
                if (mode == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && can_equip == -1) {
                    entry = item_id | ITEM_ENTRY_UNEQUIPPABLE;
                }
            }
            entries[count] = entry;
            count++;
        }
    } else {
        for (item_id = 0; item_id < ITEM_ID_END; item_id++) {
            item_category = item_get_menu_category(item_id);
            entry = item_id;
            if (primary_category != item_category && secondary_category != item_category) {
                continue;
            }
            if (fur_shop_adjust_stock(item_id, 0) == 0) {
                continue;
            }
            if (mode != ITEM_LIST_MODE_ALL) {
                can_equip = equipment_can_equip_item(unit_id, item_id);
                if (mode == ITEM_LIST_MODE_EQUIPPABLE && can_equip == -1) {
                    continue;
                }
                if (mode == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && can_equip == -1) {
                    entry = item_id | ITEM_ENTRY_UNEQUIPPABLE;
                }
            }
            entries[count] = entry;
            count++;
        }
    }

    entries[count] = -1;
    item_sort_id_list(ITEM_SORT_ORDER_STOCK, entries);
    return count;
}
