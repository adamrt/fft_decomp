#ifndef SHOP_H
#define SHOP_H

/* WORLD's shops: the outfitter's screen with its buy, sell and fitting-room modes, the fur shop and the soldier office,
 * and the item prices and stock they use. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* shop screen */
/* The shop screen's step: the index of its handler in
 * g_shop_menu_step_handlers. The ordinary shop starts at MAIN_MENU, the
 * fur shop at FUR_MENU and the soldier office at SOLDIER_OFFICE_MENU. */
typedef enum shop_step {
    SHOP_STEP_EXIT = -1, /* shop_run_screen leaves its loop */
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

extern void (*g_shop_menu_step_handlers[])(void);
extern s16 g_shop_menu_step; /* shop_step_e */
extern menu_entry_t g_shop_main_menu_thread_data;
extern u8 g_shop_main_menu_running;     /* menu open flag */
extern u8 g_shop_exit_fade_started;     /* fade-out started */
extern u8 g_shop_exit_step_initialized; /* state initialized */
extern u8 g_shop_background_visible;
extern RECT g_shop_background_rect;
extern RECT g_shop_backdrop_tile_rect;
extern RECT g_shop_edge_tile_rect;
extern RECT g_shop_background_vram_rect;
extern RECT g_shop_vram_backup_rect;

void shop_run_screen(s32 shop_type);
u32* shop_load_background(s32 index);

/* prices and stock */
/* Two-byte shop availability mask, stored most significant byte first: bit
 * 0x8000 >> location id is set when that town's outfitter sells the item. */
typedef struct shop_item_availability {
    u8 high;
    u8 low;
} shop_item_availability_t;

extern shop_item_availability_t g_shop_item_availability[];

/* The half price is clamped to at least one; the fur shop's stock to a byte. */
s32 shop_get_item_price(s16 item_id);
s32 shop_get_item_half_price(s16 item_id);
s32 fur_shop_adjust_stock(s16 item_id, s32 delta);
s32 shop_build_stock_list(s16 unit_id, s16 shop_id, s16 category, s16* entries, s32 mode);
void shop_cash_out_excess_items(void);
s32 shop_adjust_gil(s32 delta);

/* buying and selling */
/* Refusals of shop_buy_quantity_prompt and
 * fur_shop_buy_quantity_prompt; otherwise they return whether the
 * quantity prompt is still open. */
enum {
    SHOP_QUANTITY_HOLDING_MAX = -2, /* the party already holds 99 */
    SHOP_QUANTITY_CANNOT_AFFORD = -1
};

extern u8 g_shop_item_category;
extern menu_entry_t g_shop_quantity_menu_data[2];
extern u8 g_shop_quantity_menu_running;
extern s8 g_shop_buy_list_initialized;
extern s8 g_shop_buy_equip_check_pending;
extern s16 g_shop_buy_message_wait;
extern s16 g_shop_buy_item;
extern s32 g_shop_buy_unit_price;
extern u8 g_shop_buy_total_shown;
extern u8 g_shop_buy_equip_initialized;
extern u8 g_shop_buy_equip_preview_visible;
extern s16 g_shop_buy_equip_item;
extern s16 g_shop_buy_equip_last_unit;
extern u8 g_shop_buy_equip_saved_browse;
extern s8 g_shop_sell_list_initialized;
extern s16 g_shop_sell_message_wait;
extern u8 g_shop_sell_result;         /* sell-menu thread result */
extern u16 g_shop_sell_item;          /* item being sold */
extern u8 g_shop_sell_confirm_shown;  /* confirmation prompt shown */
extern u16 g_shop_sell_quantity_item; /* item being sold */
extern u8 g_shop_sell_total_shown;    /* confirmation prompt shown */

void shop_buy_equip_step(void);
void shop_sell_list_step(void);
void shop_sell_confirm_step(void);

/* fitting room */
/* Result of shop_fitting_checkout_prompt. */
typedef enum shop_fitting_checkout {
    SHOP_FITTING_CHECKOUT_DONE = -1,    /* bought, or left without buying: back to the shop's main menu */
    SHOP_FITTING_CHECKOUT_CANCELED = 0, /* back to the fitting room */
    SHOP_FITTING_CHECKOUT_PROMPT_OPEN = 1
} shop_fitting_checkout_e;

extern u8 g_shop_fitting_initialized;
extern u8 g_shop_fitting_cost_visible;
extern s16 g_shop_fitting_items[PARTY_ROSTER_SLOT_COUNT][MENU_EQUIPMENT_SLOT_COUNT];
extern s32 g_shop_fitting_cost;
extern u8 g_shop_fitting_unit_chosen;
extern s8 g_shop_fitting_item_list_open;
extern s16 g_shop_fitting_previewed_cursor;
extern s8 g_shop_fitting_item_list_close;
extern s8 g_shop_fitting_equip_menu_initialized;
extern s8 g_shop_fitting_equip_slot;
extern u16 g_shop_fitting_slot_cursor_anim;
extern point16_t g_shop_fitting_slot_cursor_point;
extern s8 g_shop_fitting_remove_preview_initialized;
extern u16 g_shop_fitting_remove_cursor_anim;
extern point16_t g_shop_fitting_remove_cursor_point;
extern menu_entry_t g_shop_fitting_sell_equipped_menu;
extern u8 g_shop_fitting_sell_equipped_menu_open;
extern u16 g_shop_fitting_sell_equipped_cursor_anim;
extern point16_t g_shop_fitting_sell_equipped_cursor_point;
extern u8 g_shop_fitting_sell_equipped_prompt_active;
extern u8 g_shop_fitting_sell_equipped_slot;
extern u8 g_shop_fitting_sell_equipped_result_shown;
extern u8 g_shop_fitting_sell_initialized;
extern s16 g_shop_fitting_sell_step; /* shop_step_e: SELL_LIST or SELL_CONFIRM */
extern menu_entry_t g_shop_fitting_checkout_confirm_menu;
extern menu_entry_t g_shop_fitting_checkout_short_gil_menu;
extern u8 g_shop_fitting_checkout_prompt_initialized;
extern menu_entry_t* g_shop_fitting_checkout_prompt_menu;
extern u8 g_shop_fitting_checkout_prompt_active;
extern u8 g_shop_fitting_checkout_prompt_result;
extern u8 g_shop_fitting_checkout_complete;
extern u8 g_shop_fitting_checkout_insufficient_gil;

/* Two-entry menu chain: row 3 of [0] opens [1] through [0]'s parent-index table. */
extern menu_entry_t g_shop_fitting_menu_thread_data[2];

/* Last equipment slot whose stat preview was built, -1 when none. The
 * ATTACK overlay has an unrelated 16-bit value at this same address. */
extern s8 g_shop_fitting_remove_previewed_equip_slot;
extern s16 g_shop_fitting_remove_previewed_unit_index;

/* The best-fit confirmation menu entry, followed directly by its open/closed
 * gate byte, as g_shop_fitting_sell_equipped_menu / _open are paired. */
extern menu_entry_t g_shop_fitting_best_fit_menu;
extern s8 g_shop_fitting_best_fit_menu_open;

void shop_fitting_build_best_fit(s16 unit_index, s16 shop_id, s16* out);

/* fur shop */
extern menu_entry_t g_fur_shop_menu_thread_data;
extern u8 g_fur_shop_opened; /* fur shop opened */
extern s8 g_fur_shop_access_granted;
extern u8 g_fur_shop_has_secret_hunt;   /* any unit has Secret Hunt */
extern u8 g_fur_shop_has_poached_stock; /* any poached item in stock */
extern u8 g_fur_shop_waiting_message;   /* waiting for message dismissal */
extern s8 g_fur_shop_purchase_list_initialized;
extern s8 g_fur_shop_equip_check_pending;
extern s16 g_fur_shop_purchase_message_wait;
extern u8 g_fur_shop_purchase_result;        /* buy quantity menu thread (menu_start_managed_thread) */
extern u16 g_fur_shop_purchase_item;         /* item being bought */
extern s32 g_fur_shop_purchase_price;        /* half the item price */
extern u8 g_fur_shop_purchase_confirm_shown; /* confirmation prompt shown */
extern s8 g_fur_shop_sell_list_initialized;
extern s16 g_fur_shop_sale_message_wait;
extern u8 g_fur_shop_quantity_menu_running; /* sell quantity menu thread (menu_start_managed_thread) */

/* soldier office */
/* Prefix of the shop roster display stream, through its first number command. */
typedef struct soldier_office_unit_count_script {
    menu_draw_state_command_t priority;       /* 0x00 */
    menu_texture_page_command_t texture_page; /* 0x04 */
    menu_fixed_clut_command_t clut;           /* 0x09 */
    menu_sprite_command_t sprites[2];         /* 0x0e */
    menu_draw_number_command_t unit_count;    /* 0x20 */
} soldier_office_unit_count_script_t;
typedef char
    soldier_office_unit_count_script_size_must_be_42[(sizeof(soldier_office_unit_count_script_t) == 42) ? 1 : -1];

extern u8 g_soldier_office_hire_menu_initialized;
extern u8 g_soldier_office_hire_unread_flag_1; /* cleared when the Soldier Office hire menu opens; never read */
extern u8 g_soldier_office_hire_unread_flag_2; /* as g_soldier_office_hire_unread_flag_1 */
extern menu_entry_t g_soldier_office_hire_confirm_menu;
extern u8 g_soldier_office_hire_confirm_message_wait;
extern menu_entry_t g_soldier_office_hire_fee_confirm_menu;
extern u8 g_soldier_office_hire_fee_message_wait;
extern u8 g_soldier_office_hire_name_entry_initialized;
extern u8 g_soldier_office_hire_result_shown;
extern u8 g_soldier_office_hire_preview_initialized;
extern u8 g_soldier_office_rename_prompt_initialized;
extern u8 g_soldier_office_rename_name_entry_initialized;
extern u8 g_soldier_office_rename_error_shown;
extern const u8* g_soldier_office_entered_name;
extern u32 g_soldier_office_fee;
extern soldier_office_unit_count_script_t g_soldier_office_unit_count_script;
extern u8 g_soldier_office_unit_count;

/* The cursor doubles as the soldier-office mode (soldier_office_get_fee). */
extern menu_entry_t g_soldier_office_hire_menu_thread_data;

#endif
