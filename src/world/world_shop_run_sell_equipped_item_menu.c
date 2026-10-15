#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Run one frame of the shop's sell-equipped-item menu.
 *
 * The cursor walks the selected unit's five equipment slots; confirming a
 * slot the unit owns (non-empty, not taken from the shop) opens a sale prompt
 * for half the item's price and, on acceptance, credits the gil and clears
 * the slot. Triangle returns to shop step 6.
 */
void world_shop_run_sell_equipped_item_menu(void) {
    s32 close;
    u32 item;
    s32 text_id;
    u32 input;

    close = 0;
    if (g_world_shop_sell_equipped_menu_open == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80c, 0);
            g_world_shop_sell_equipped_prompt_active = 0;
            g_world_formation_unit_cycle_mode = 1;
            g_world_shop_sell_equipped_menu_open = 1;
            g_world_shop_sell_equipped_result_shown = 0;
        }
    } else {
        if (g_world_shop_sell_equipped_result_shown != 0) {
            if (g_menu_input_primary_repeat
                & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80d, 0);
                g_world_shop_sell_equipped_result_shown = 0;
            }
        } else if (g_world_shop_sell_equipped_prompt_active == 0) {
            g_world_shop_sell_equipped_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
                WORLD_MENU_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
            if (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                != ITEM_ID_NOTHING) {
                text_id = (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                              & WORLD_ITEM_ID_MASK)
                    + 0x6800;
            } else {
                text_id = -1;
            }
            input = g_menu_input_primary_repeat;
            g_menu_help_message_text_id = text_id;
            if (input & PSX_PAD_CIRCLE) {
                item = g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot];
                if (item == ITEM_ID_NOTHING || (item >> 15) != 0) {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    g_world_shop_sell_equipped_prompt_active = 1;
                    g_world_shop_sell_equipped_menu.selected_index = 0;
                    g_text_substitution_values[0] = world_item_get_half_price(item);
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf810, 0);
                }
            } else if (input & PSX_PAD_CROSS) {
                close = 1;
            }
        } else {
            g_menu_help_message_text_id = 0;
            g_sound_suppressed = g_world_shop_sell_equipped_prompt_active
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_sell_equipped_menu);
            if (g_world_shop_sell_equipped_prompt_active == 0) {
                if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf811, 0);
                    world_shop_adjust_gil(g_text_substitution_values[0]);
                    g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                        = ITEM_ID_NOTHING;
                    world_formation_recalculate_selected_unit_stats();
                    g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
                } else {
                    if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
                        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    }
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80a, 0);
                }
                g_world_shop_sell_equipped_result_shown = 1;
            }
        }
        if (close != 0) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_cycle_mode = 2;
            g_world_shop_sell_equipped_menu_open = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        g_world_shop_sell_equipped_cursor_point.y = g_world_shop_sell_equipped_slot * 16 + 0x90;
        gfx_set_otag_index(0x3d);
        menu_update_and_draw_sliding_cursor_variant(&g_world_shop_sell_equipped_cursor_point,
            &g_world_shop_sell_equipped_cursor_anim, world_thread_is_task_active(0));
    }
    world_shop_install_callbacks_and_run();
}
