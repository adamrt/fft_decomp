#ifndef WORLD_SHOP_H
#define WORLD_SHOP_H

/* WORLD's shops: the outfitter's screen, buying and selling, the fitting room, the fur shop and the soldier office. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/menu_status.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* shop screen */
/* The shop screen's step: the index of its handler in
 * g_world_shop_menu_step_handlers. The ordinary shop starts at MAIN_MENU, the
 * fur shop at FUR_MENU and the soldier office at SOLDIER_OFFICE_MENU. */
typedef enum world_shop_step {
    WORLD_SHOP_STEP_EXIT = -1, /* world_shop_run_screen leaves its loop */
    WORLD_SHOP_STEP_MAIN_MENU = 0,
    WORLD_SHOP_STEP_BUY_LIST = 1,
    WORLD_SHOP_STEP_BUY_CONFIRM = 2,
    WORLD_SHOP_STEP_BUY_EQUIP = 3, /* who can equip the item under the cursor */
    WORLD_SHOP_STEP_SELL_LIST = 4,
    WORLD_SHOP_STEP_SELL_CONFIRM = 5,
    WORLD_SHOP_STEP_FITTING_ROOM = 6, /* unit grid, then the fitting menu */
    WORLD_SHOP_STEP_FITTING_ROOM_EQUIP = 7,
    WORLD_SHOP_STEP_FITTING_ROOM_BEST_FIT = 8,
    WORLD_SHOP_STEP_FITTING_ROOM_REMOVE = 9,
    WORLD_SHOP_STEP_FITTING_ROOM_SELL_EQUIPPED = 10,
    WORLD_SHOP_STEP_FITTING_ROOM_SELL = 11, /* runs the SELL_LIST and SELL_CONFIRM handlers */
    WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT = 12,
    WORLD_SHOP_STEP_EXIT_FADE = 13,
    WORLD_SHOP_STEP_FUR_MENU = 14,
    WORLD_SHOP_STEP_FUR_BUY_LIST = 15, /* the shop's poached stock */
    WORLD_SHOP_STEP_FUR_BUY_CONFIRM = 16,
    WORLD_SHOP_STEP_FUR_BUY_EQUIP = 17,
    WORLD_SHOP_STEP_FUR_SELL_LIST = 18, /* the party's inventory */
    WORLD_SHOP_STEP_FUR_SELL_CONFIRM = 19,
    WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU = 20,
    WORLD_SHOP_STEP_HIRE_PREVIEW = 21,
    WORLD_SHOP_STEP_HIRE_CONFIRM = 22, /* declining removes the recruit from the roster */
    WORLD_SHOP_STEP_HIRE_NAME_ENTRY = 23,
    WORLD_SHOP_STEP_HIRE_FEE_CONFIRM = 24,
    WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN = 25,
    WORLD_SHOP_STEP_RENAME_SELECT_UNIT = 26,
    WORLD_SHOP_STEP_RENAME_NAME_ENTRY = 27,
} world_shop_step_e;

extern void (*g_world_shop_menu_step_handlers[])(void);
extern s16 g_world_shop_menu_step; /* world_shop_step_e */
extern menu_entry_t g_world_shop_main_menu_thread_data;
extern u8 g_world_shop_main_menu_running;     /* menu open flag */
extern u8 g_world_shop_exit_fade_started;     /* fade-out started */
extern u8 g_world_shop_exit_step_initialized; /* state initialized */
/* Sector and size pairs, read as flat 32-bit values ([i * 2], [i * 2 + 1]): a record
 * member access changes how the target addresses them. */
extern s32 g_world_shop_background_tim_files[9 * 2];
extern u8 g_world_shop_background_visible;
extern RECT g_world_shop_background_rect;
extern RECT g_world_shop_backdrop_tile_rect;
extern RECT g_world_shop_edge_tile_rect;
extern RECT g_world_shop_background_vram_rect;
extern RECT g_world_shop_vram_backup_rect;
extern u8 g_world_shop_cost_window_visible;
extern u8 g_world_unknown_801cd0ec; /* cleared when the Soldier Office hire menu opens; never read */
extern u8 g_world_unknown_801cd0f4; /* as g_world_unknown_801cd0ec */
void world_shop_run_screen(s32 shop_type);
u32* world_shop_load_background_tim(s32 index);
void world_shop_update_unit_selection(void);
s32 world_shop_adjust_gil(s32 delta);

/* stock */
/* Two-byte shop availability mask, stored most significant byte first: bit
 * 0x8000 >> location id is set when that town's outfitter sells the item. */
typedef struct world_shop_item_availability {
    u8 high;
    u8 low;
} world_shop_item_availability_t;

extern world_shop_item_availability_t g_world_shop_item_availability[];
extern u8 g_world_shop_item_category;
s32 world_shop_build_item_list(s16 unit_id, s16 shop_id, s16 category, s16* entries, s32 mode);
s32 world_shop_adjust_poached_item_quantity(s16 item_id, s32 delta);

/* buying and selling */
/* Refusals of world_shop_run_item_purchase_quantity_menu and
 * world_shop_run_fur_buy_quantity_menu; otherwise they return whether the
 * quantity prompt is still open. */
enum {
    WORLD_SHOP_QUANTITY_HOLDING_MAX = -2, /* the party already holds 99 */
    WORLD_SHOP_QUANTITY_CANNOT_AFFORD = -1,
};

extern s16 g_world_shop_quantity_limit;
extern menu_entry_t g_world_shop_quantity_menu_data[2];
extern u8 g_world_shop_quantity_menu_running;
extern menu_instruction_t g_world_shop_obtain_gil_menu_script;
extern menu_instruction_t g_world_shop_purchase_menu_script;
extern s8 g_world_shop_purchase_list_initialized;
extern s8 g_world_shop_purchase_equip_check_pending;
extern s16 g_world_shop_purchase_message_wait;
extern s16 g_world_shop_purchase_item;
extern s32 g_world_shop_purchase_unit_price;
extern u8 g_world_shop_purchase_total_shown;
extern u8 g_world_shop_equip_candidate_initialized;
extern u8 g_world_shop_equip_candidate_preview_visible;
extern s16 g_world_shop_equip_candidate_item;
extern s16 g_world_shop_equip_candidate_last_unit;
extern u8 g_world_shop_equip_candidate_saved_browse;
extern menu_instruction_t g_world_shop_sell_list_menu_script;
extern s8 g_world_shop_sell_list_initialized;
extern s16 g_world_shop_sell_message_wait;
extern u8 g_world_shop_sell_result;         /* sell-menu thread result */
extern u16 g_world_shop_sell_item;          /* item being sold */
extern u8 g_world_shop_sell_confirm_shown;  /* confirmation prompt shown */
extern u16 g_world_shop_sell_quantity_item; /* item being sold */
extern u8 g_world_shop_sell_total_shown;    /* confirmation prompt shown */
extern menu_instruction_t g_world_shop_sale_confirm_menu_script;
void world_shop_run_equip_candidate_step(void);
void world_shop_run_item_sell_list_step(void);
void world_shop_run_item_sell_confirm_step(void);

/* fitting room */
/* Result of world_shop_run_fitting_room_checkout_prompt. */
typedef enum world_shop_checkout_result {
    WORLD_SHOP_CHECKOUT_DONE = -1,    /* bought, or left without buying: back to the shop's main menu */
    WORLD_SHOP_CHECKOUT_CANCELED = 0, /* back to the fitting room */
    WORLD_SHOP_CHECKOUT_PROMPT_OPEN = 1,
} world_shop_checkout_result_e;

extern u8 g_world_shop_fitting_room_initialized;
/* Two-entry menu chain: row 3 of [0] opens [1] through [0]'s parent-index table. */
extern menu_entry_t g_world_shop_fitting_room_menu_thread_data[2];
extern s16 g_world_shop_fitting_room_items[PARTY_ROSTER_SLOT_COUNT][MENU_EQUIPMENT_SLOT_COUNT];
extern s32 g_world_shop_fitting_room_cost;
extern u8 g_world_shop_fitting_room_unit_chosen;
extern s8 g_world_shop_fitting_room_item_list_open;
extern s16 g_world_shop_fitting_room_previewed_cursor;
extern menu_instruction_t g_world_shop_fitting_item_list_menu_script;
extern s8 g_world_shop_fitting_item_list_close;
extern s8 g_world_shop_fitting_equip_menu_initialized;
extern s8 g_world_shop_fitting_equip_slot;
extern u16 g_world_shop_fitting_slot_cursor_anim;
extern point16_t g_world_shop_fitting_slot_cursor_point;
extern menu_instruction_t g_world_shop_fitting_cost_menu_script;
extern s8 g_world_shop_equip_slot_preview_initialized;
extern u16 g_world_shop_equip_slot_cursor_anim;
extern point16_t g_world_shop_equip_slot_cursor_point;
/* Last equipment slot whose stat preview was built, -1 when none. The
 * ATTACK overlay has an unrelated 16-bit value at this same address. */
extern s8 g_world_shop_previewed_equip_slot;
extern s16 g_world_shop_previewed_unit_index;
/* The best-fit confirmation menu entry, followed directly by its open/closed
 * gate byte, as g_world_shop_sell_equipped_menu / _open are paired. */
extern menu_entry_t g_world_shop_best_fit_menu;
extern s8 g_world_shop_best_fit_menu_open;
/* Second slot-category table of world_shop_build_fitting_room_best_fit_equipment. */
extern u8 g_world_fitting_room_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_COUNT];
extern menu_entry_t g_world_shop_sell_equipped_menu;
extern u8 g_world_shop_sell_equipped_menu_open;
extern u16 g_world_shop_sell_equipped_cursor_anim;
extern point16_t g_world_shop_sell_equipped_cursor_point;
extern u8 g_world_shop_sell_equipped_prompt_active;
extern u8 g_world_shop_sell_equipped_slot;
extern u8 g_world_shop_sell_equipped_result_shown;
extern u8 g_world_shop_sell_subflow_initialized;
extern s16 g_world_shop_sell_subflow_step; /* world_shop_step_e: SELL_LIST or SELL_CONFIRM */
extern menu_entry_t g_world_shop_checkout_confirm_menu;
extern menu_entry_t g_world_shop_checkout_short_gil_menu;
extern u8 g_world_shop_checkout_prompt_initialized;
extern menu_entry_t* g_world_shop_checkout_prompt_menu;
extern u8 g_world_shop_checkout_prompt_active;
extern u8 g_world_shop_checkout_prompt_result;
extern u8 g_world_shop_checkout_complete;
extern u8 g_world_shop_checkout_insufficient_gil;
void world_shop_build_fitting_room_best_fit_equipment(s16 unit_index, s16 shop_id, s16* out);

/* fur shop */
extern menu_entry_t g_world_shop_fur_menu_thread_data;
extern u8 g_world_shop_fur_opened; /* fur shop opened */
extern s8 g_world_shop_fur_access_granted;
extern u8 g_world_shop_fur_has_secret_hunt;   /* any unit has Secret Hunt */
extern u8 g_world_shop_fur_has_poached_stock; /* any poached item in stock */
extern u8 g_world_shop_fur_waiting_message;   /* waiting for message dismissal */
extern menu_instruction_t g_world_shop_fur_purchase_menu_script;
extern s8 g_world_shop_fur_purchase_list_initialized;
extern s8 g_world_shop_fur_equip_check_pending;
extern s16 g_world_shop_fur_purchase_message_wait;
extern u8 g_world_shop_fur_purchase_result;        /* quantity-menu thread result */
extern u16 g_world_shop_fur_purchase_item;         /* item being bought */
extern s32 g_world_shop_fur_purchase_price;        /* half the item price */
extern u8 g_world_shop_fur_purchase_confirm_shown; /* confirmation prompt shown */
extern s8 g_world_shop_fur_sell_list_initialized;
extern s16 g_world_shop_fur_sale_message_wait;
extern u8 g_world_shop_fur_quantity_menu_running; /* sell-menu thread result */

/* soldier office */
/* Prefix of the shop roster display stream, through its first number command. */
typedef struct world_shop_roster_count_menu_script {
    menu_draw_state_command_t priority;       /* 0x00 */
    menu_texture_page_command_t texture_page; /* 0x04 */
    menu_fixed_clut_command_t clut;           /* 0x09 */
    menu_sprite_command_t sprites[2];         /* 0x0e */
    menu_draw_number_command_t unit_count;    /* 0x20 */
} world_shop_roster_count_menu_script_t;
typedef char
    world_shop_roster_count_menu_script_size_must_be_42[(sizeof(world_shop_roster_count_menu_script_t) == 42) ? 1 : -1];

/* The cursor doubles as the soldier-office mode (world_shop_get_soldier_office_entry_fee). */
extern menu_entry_t g_world_shop_hire_menu_thread_data;
extern u8 g_world_shop_hire_menu_initialized;
extern menu_entry_t g_world_shop_hire_confirm_menu;
extern u8 g_world_shop_hire_confirm_message_wait;
extern menu_entry_t g_world_shop_hire_fee_confirm_menu;
extern u8 g_world_shop_hire_fee_message_wait;
extern u8 g_world_shop_hire_name_entry_initialized;
extern u8 g_world_shop_hire_result_shown;
extern u8 g_world_shop_recruit_preview_initialized;
extern u8 g_world_shop_rename_prompt_initialized;
extern u8 g_world_shop_rename_name_entry_initialized;
extern u8 g_world_shop_rename_error_shown;
extern const u8* g_world_shop_entered_unit_name;
extern menu_instruction_t g_world_shop_soldier_office_fee_menu_script;
extern u32 g_world_shop_service_fee;
extern world_shop_roster_count_menu_script_t g_world_shop_roster_count_menu_script;
extern u8 g_world_shop_party_unit_count;

#endif
