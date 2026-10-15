#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Fitting-room counterpart of world_menu_run_remove_item: takes one equipment
 * slot off the selected unit, refunding a shop item's price from the fitting
 * room cost, and previews the stat change of the slot under the cursor
 * against the unit's own item held in the fitting room.
 *
 * The prologue's $a0 is not an argument register here. The callee at
 * 0x80114d4c is world_shop_update_unit_selection, which takes no
 * arguments; $a0 simply holds the copy of the selected-unit index that the
 * store to g_world_shop_previewed_unit_index consumes, while the compare keeps the load in $v1.
 * A one-argument prototype would pull the load pseudo into $a0 and push the
 * copy into $v1; pins and launders cannot compensate, because the target's
 * copy sits in the beq's delay slot and a tied asm is never a delay-slot
 * candidate. */
void world_shop_run_fitting_room_remove_step(void) {
    /* The target reserves 64 local bytes whose contents are not accessed;
     * saved registers sit at 0x60..0x70 under a -0x78 frame. */
    s32 unused[16];
    s8 slot;
    s32 index;
    s32 value;
    s32 equipped_item;
    s16* cursor_y;
    s16 unit;

    if (g_world_shop_previewed_unit_index != g_unit_selected_index) {
        unit = g_unit_selected_index;
        g_world_shop_previewed_unit_index = unit;
        g_world_shop_equip_slot_preview_initialized = 0;
        world_shop_update_unit_selection();
        menu_input_clear_state();
    }
    if (g_world_shop_equip_slot_preview_initialized == 0) {
        g_world_shop_previewed_equip_slot = -1;
        g_world_shop_equip_slot_preview_initialized = 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        g_world_shop_equip_slot_preview_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
    }
    slot = menu_update_vertical_selection_and_mark_change(
        WORLD_EQUIPMENT_SLOT_COUNT, WORLD_MENU_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
    if (g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] != ITEM_ID_NOTHING) {
        value = (g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] & WORLD_ITEM_ID_MASK) + 0x6800;
    } else {
        value = -1;
    }
    g_world_menu_description_text_id = value;
    cursor_y = &g_world_shop_equip_slot_cursor_point.y;
    *cursor_y = (u8)slot * 16 + 0x90;
    world_menu_set_draw_priority(0x3D);
    world_menu_draw_animated_cursor(
        &g_world_shop_equip_slot_cursor_point, &g_world_shop_equip_slot_cursor_anim, world_thread_is_task_active(0));
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        equipped_item = g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot];
        if ((equipped_item >> 15) == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            world_shop_add_fitting_room_cost(-world_item_get_price(equipped_item & WORLD_ITEM_ID_MASK));
            world_shop_finalize_unit_equips_from_fitting_room(g_unit_selected_index, slot & 0xFF);
            world_formation_recalculate_selected_unit_stats();
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
            g_world_shop_previewed_equip_slot = -1;
        }
    }
    index = (u8)slot;
    if (g_world_shop_previewed_equip_slot != index) {
        equipped_item = g_world_unit_pointers[g_unit_selected_index]->equipment[index];
        g_world_shop_previewed_equip_slot = slot;
        if ((equipped_item >> 15) != 0) {
            world_menu_toggle_preview_stats_window(1);
            menu_calculate_swap_stat_delta(&g_world_item_preview_stat_detail, &g_world_selected_unit_stat_summary,
                equipped_item, g_world_shop_fitting_room_items[g_unit_selected_index][index], index);
            g_world_preview_stats_thread_params.redraw_request = 1;
        } else {
            world_menu_toggle_preview_stats_window(0);
        }
        g_menu_status_display_thread_params.redraw_request = 1;
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}
