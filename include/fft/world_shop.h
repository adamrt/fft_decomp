#ifndef WORLD_SHOP_H
#define WORLD_SHOP_H

/* WORLD's shops: outfitters, the fitting room, the fur shop and the soldier office. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* gfx */
/* Sector and size pairs, read as flat 32-bit values ([i * 2], [i * 2 + 1]): a record
 * member access changes how the target addresses them. */
extern s32 g_world_shop_background_tim_files[9 * 2];

/* formation */
extern menu_entry_t g_world_shop_hire_confirm_menu;
extern u8 g_world_shop_hire_confirm_message_wait;

/* shop */
extern s16 g_world_shop_fitting_room_items[PARTY_ROSTER_SLOT_COUNT][MENU_EQUIPMENT_SLOT_COUNT];
extern u8 g_world_shop_hire_menu_initialized;
extern u8 g_world_shop_item_category;
extern world_shop_item_availability_t g_world_shop_item_availability[];
extern void (*g_world_shop_menu_step_handlers[])(void);
extern s16 g_world_shop_menu_step; /* world_shop_step_e */
extern s16 g_world_shop_quantity_limit;
extern menu_entry_t g_world_shop_quantity_menu_data[2];
extern u8 g_world_shop_background_visible;
extern menu_entry_t g_world_shop_main_menu_thread_data;
extern menu_entry_t g_world_shop_fur_menu_thread_data;
/* The cursor doubles as the soldier-office mode (world_shop_get_soldier_office_entry_fee). */
extern menu_entry_t g_world_shop_hire_menu_thread_data;
extern RECT g_world_shop_background_rect;
extern RECT g_world_shop_backdrop_tile_rect;
extern RECT g_world_shop_edge_tile_rect;
extern RECT g_world_shop_background_vram_rect;
extern RECT g_world_shop_vram_backup_rect;
extern u8 g_world_shop_main_menu_running;     /* menu open flag */
extern u8 g_world_shop_exit_fade_started;     /* fade-out started */
extern u8 g_world_shop_exit_step_initialized; /* state initialized */
extern menu_instruction_t g_world_shop_obtain_gil_menu_script;
extern menu_instruction_t g_world_shop_purchase_menu_script;
extern s8 g_world_shop_purchase_list_initialized;
extern s8 g_world_shop_purchase_equip_check_pending;
extern s16 g_world_shop_purchase_message_wait;
extern u8 g_world_shop_quantity_menu_running;
extern u8 g_world_shop_equip_candidate_initialized;
extern u8 g_world_shop_equip_candidate_preview_visible;
extern menu_instruction_t g_world_shop_sell_list_menu_script;
extern s8 g_world_shop_sell_list_initialized;
extern s16 g_world_shop_sell_message_wait;
extern u8 g_world_shop_sell_result; /* sell-menu thread result */
extern u8 g_world_shop_fitting_room_initialized;
/* Two-entry menu chain: row 3 of [0] opens [1] through [0]'s parent-index
 * table at 0x80195518. */
extern menu_entry_t g_world_shop_fitting_room_menu_thread_data[2];
extern menu_instruction_t g_world_shop_fitting_item_list_menu_script;
extern s8 g_world_shop_fitting_equip_menu_initialized;
extern s8 g_world_shop_fitting_equip_slot;
extern s8 g_world_shop_fitting_item_list_close;
extern u16 g_world_shop_fitting_slot_cursor_anim;
extern point16_t g_world_shop_fitting_slot_cursor_point;
extern menu_instruction_t g_world_shop_fitting_cost_menu_script;
extern s8 g_world_shop_equip_slot_preview_initialized;
extern u16 g_world_shop_equip_slot_cursor_anim;
extern point16_t g_world_shop_equip_slot_cursor_point;
extern menu_entry_t g_world_shop_checkout_confirm_menu;
extern menu_entry_t g_world_shop_checkout_short_gil_menu;
extern u8 g_world_shop_checkout_prompt_initialized;
/* The best-fit confirmation menu entry, followed directly by its open/closed
 * gate byte, as g_world_shop_sell_equipped_menu / _open are paired. */
extern menu_entry_t g_world_shop_best_fit_menu;
extern s8 g_world_shop_best_fit_menu_open;
extern menu_entry_t g_world_shop_sell_equipped_menu;
extern u8 g_world_shop_sell_equipped_menu_open;
extern u16 g_world_shop_sell_equipped_cursor_anim;
extern point16_t g_world_shop_sell_equipped_cursor_point;
extern u8 g_world_shop_sell_subflow_initialized;
extern u8 g_world_shop_fur_opened; /* fur shop opened */
extern menu_instruction_t g_world_shop_sale_confirm_menu_script;
extern s8 g_world_shop_fur_sell_list_initialized;
extern s16 g_world_shop_fur_sale_message_wait;
extern u8 g_world_shop_fur_quantity_menu_running; /* sell-menu thread result */
extern menu_instruction_t g_world_shop_fur_purchase_menu_script;
extern s8 g_world_shop_fur_purchase_list_initialized;
extern s8 g_world_shop_fur_equip_check_pending;
extern s16 g_world_shop_fur_purchase_message_wait;
extern u8 g_world_shop_fur_purchase_result; /* quantity-menu thread result */
extern menu_entry_t g_world_shop_hire_fee_confirm_menu;
extern u8 g_world_shop_recruit_preview_initialized;
extern u8 g_world_shop_hire_name_entry_initialized;
extern u8 g_world_shop_hire_fee_message_wait;
extern u8 g_world_shop_rename_prompt_initialized;
extern u8 g_world_shop_rename_name_entry_initialized;
extern menu_instruction_t g_world_shop_soldier_office_fee_menu_script;
extern world_shop_roster_count_menu_script_t g_world_shop_roster_count_menu_script;
extern s32 g_world_shop_purchase_unit_price;
extern s16 g_world_shop_purchase_item;
extern u8 g_world_shop_purchase_total_shown;
extern s16 g_world_shop_equip_candidate_last_unit;
extern u8 g_world_shop_equip_candidate_saved_browse;
extern s16 g_world_shop_equip_candidate_item;
extern u16 g_world_shop_sell_item;         /* item being sold */
extern u8 g_world_shop_sell_confirm_shown; /* confirmation prompt shown */
extern s32 g_world_shop_fitting_room_cost;
extern u8 g_world_shop_fitting_room_unit_chosen;
extern s8 g_world_shop_fitting_room_item_list_open;
extern s16 g_world_shop_fitting_room_previewed_cursor;
/* Last equipment slot whose stat preview was built, -1 when none. The
 * ATTACK overlay has an unrelated 16-bit value at this same address. */
extern s8 g_world_shop_previewed_equip_slot;
extern s16 g_world_shop_previewed_unit_index;
extern u8 g_world_shop_checkout_complete;
extern u8 g_world_shop_checkout_insufficient_gil;
extern u8 g_world_shop_checkout_prompt_active;
extern u8 g_world_shop_checkout_prompt_result;
extern menu_entry_t* g_world_shop_checkout_prompt_menu;
extern u8 g_world_shop_sell_equipped_prompt_active;
extern u8 g_world_shop_sell_equipped_slot;
extern u8 g_world_shop_sell_equipped_result_shown;
extern s16 g_world_shop_sell_subflow_step;         /* world_shop_step_e: SELL_LIST or SELL_CONFIRM */
extern u8 g_world_shop_fur_waiting_message;        /* waiting for message dismissal */
extern u8 g_world_shop_fur_has_secret_hunt;        /* any unit has Secret Hunt */
extern u8 g_world_shop_fur_has_poached_stock;      /* any poached item in stock */
extern u16 g_world_shop_sell_quantity_item;        /* item being sold */
extern u8 g_world_shop_sell_total_shown;           /* confirmation prompt shown */
extern s32 g_world_shop_fur_purchase_price;        /* half the item price */
extern u16 g_world_shop_fur_purchase_item;         /* item being bought */
extern u8 g_world_shop_fur_purchase_confirm_shown; /* confirmation prompt shown */
extern s8 g_world_shop_fur_access_granted;
extern u8 g_world_shop_hire_result_shown;
extern u8 g_world_shop_rename_error_shown;
extern u8 g_world_shop_party_unit_count;
extern const u8* g_world_shop_entered_unit_name;
extern u32 g_world_shop_service_fee;
extern u8 g_world_shop_cost_window_visible;
void world_shop_run_item_sell_list_step(void);
void world_shop_run_item_sell_confirm_step(void);
s32 world_shop_adjust_poached_item_quantity(s16 item_id, s32 delta);
void world_shop_build_fitting_room_best_fit_equipment(s16 unit_index, s16 shop_id, s16* out);
s32 world_shop_build_item_list(s16 unit_id, s16 shop_id, s16 category, s16* entries, s32 mode);
s32 world_shop_adjust_gil(s32 delta);
void world_shop_run_equip_candidate_step(void);
void world_shop_update_unit_selection(void);
void world_shop_run_screen(s32 shop_type);
u32* world_shop_load_background_tim(s32 index);

/* other */
/* Second slot-category table of world_shop_build_fitting_room_best_fit_equipment. */
extern u8 g_world_fitting_room_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_COUNT];

#endif
