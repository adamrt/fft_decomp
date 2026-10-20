#ifndef SHOP_H
#define SHOP_H

/* WORLD's shops: the shop's screen with its buy, sell and fitting-room modes, the fur shop and the soldier office,
 * and the item prices and stock they use. */

#include "fft/menu.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* shop screen */
/* The shop screen's step: the index of its handler in
 * g_shop_step_handlers. The ordinary shop starts at MAIN_MENU, the
 * fur shop at FUR_MENU and the soldier office at SOLDIER_OFFICE_MENU. */
typedef enum shop_step {
    SHOP_STEP_EXIT = -1, /* shop_screen_run leaves its loop */
    SHOP_STEP_MAIN_MENU = 0,
    SHOP_STEP_BUY_LIST = 1,
    SHOP_STEP_BUY_CONFIRM = 2,
    SHOP_STEP_BUY_EQUIP = 3, /* who can equip the item under the cursor */
    SHOP_STEP_SELL_LIST = 4,
    SHOP_STEP_SELL_CONFIRM = 5,
    SHOP_STEP_FITTING_ROOM = 6, /* unit grid, then the fitting menu */
    SHOP_STEP_FITTING_ROOM_EQUIP = 7,
    SHOP_STEP_FITTING_ROOM_BEST_FIT = 8,
    SHOP_STEP_FITTING_ROOM_REMOVE = 9,
    SHOP_STEP_FITTING_ROOM_SELL_EQUIPPED = 10,
    SHOP_STEP_FITTING_ROOM_SELL = 11, /* runs the SELL_LIST and SELL_CONFIRM handlers */
    SHOP_STEP_FITTING_ROOM_CHECKOUT = 12,
    SHOP_STEP_EXIT_FADE = 13,
    SHOP_STEP_FUR_MENU = 14,
    SHOP_STEP_FUR_BUY_LIST = 15, /* the shop's poached stock */
    SHOP_STEP_FUR_BUY_CONFIRM = 16,
    SHOP_STEP_FUR_BUY_EQUIP = 17,
    SHOP_STEP_FUR_SELL_LIST = 18, /* the party's inventory */
    SHOP_STEP_FUR_SELL_CONFIRM = 19,
    SHOP_STEP_SOLDIER_OFFICE_MENU = 20,
    SHOP_STEP_HIRE_PREVIEW = 21,
    SHOP_STEP_HIRE_CONFIRM = 22, /* declining removes the recruit from the roster */
    SHOP_STEP_HIRE_NAME_ENTRY = 23,
    SHOP_STEP_HIRE_FEE_CONFIRM = 24,
    SHOP_STEP_SOLDIER_OFFICE_REOPEN = 25,
    SHOP_STEP_RENAME_SELECT_UNIT = 26,
    SHOP_STEP_RENAME_NAME_ENTRY = 27
} shop_step_e;

extern s16 g_shop_menu_step; /* shop_step_e */

void shop_screen_run(s32 shop_type);
u32* shop_background_load(s32 index);
void shop_gil_draw(void);
void shop_switch_item_tab(s32 input);

/* buying and selling */
s32 shop_buy_owned_count_get(s32 unused_row);
s32 shop_buy_equipped_count_get(s32 unused_row);
s32 shop_buy_price_get(s32 entry_index);
void shop_buy_unit_preview_update(void);
s32 shop_sell_price_get(s32 unused_row);
void shop_sell_list_update(void);
void shop_sell_confirm_update(void);

/* prices and stock */
/* The half price is clamped to at least one; the fur shop's stock to a byte. */
s32 shop_item_price_get(s16 item_id);
s32 shop_item_half_price_get(s16 item_id);
s32 fur_shop_stock_adjust(s16 item_id, s32 delta);
s32 shop_stock_list_build(s16 unit_id, s16 shop_id, s16 category, s16* entries, s32 mode);
void shop_cash_out_excess_items(void);
s32 shop_gil_adjust(s32 delta);

/* fitting room */
extern s16 g_shop_fitting_items[PARTY_ROSTER_SLOT_COUNT][MENU_EQUIPMENT_SLOT_COUNT];

void shop_fitting_best_fit_build(s16 unit_index, s16 shop_id, s16* out);
void shop_fitting_menu_update(void);
void shop_fitting_equipment_update(void);
void shop_fitting_remove_update(void);
void shop_fitting_checkout_update(void);
void shop_fitting_best_fit_update(void);
void shop_fitting_equipped_sale_update(void);
void shop_fitting_inventory_sale_update(void);

/* fur shop */
void fur_shop_menu_update(void);
void fur_shop_sell_list_update(void);
void fur_shop_sell_confirm_update(void);
void fur_shop_buy_list_update(void);
void fur_shop_buy_confirm_update(void);
void fur_shop_buy_unit_preview_update(void);

/* soldier office */
void soldier_office_menu_update(void);
void soldier_office_hire_preview_update(void);
void soldier_office_hire_confirm_update(void);
void soldier_office_hire_name_update(void);
void soldier_office_hire_fee_update(void);
void soldier_office_menu_reopen(void);
void soldier_office_rename_unit_update(void);
void soldier_office_rename_name_update(void);
void soldier_office_fee_draw(void);
void soldier_office_unit_count_draw(void);

#endif
