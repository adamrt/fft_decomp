#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Run one frame of the shop's "who can equip" unit browser for the item
 * under the list cursor.
 *
 * Moving onto a unit that can equip the item stages it in the temporary
 * formation preview record and shows the stat preview; slots
 * already holding the item are flagged in g_world_selected_unit_stat_detail.state.equipment. Cancel returns to shop
 * step 1.
 */
void world_shop_run_equip_candidate_step(void) {
    s32 unused[16];
    s32 input;
    u8 equipped;
    s32 i;
    s32 slot;
    s32 text_id;

    input = g_menu_input_secondary_repeat;
    if (g_world_shop_equip_candidate_initialized == 0) {
        menu_set_vsync_mode_and_event_speed(0);
        g_world_formation_unit_browse_enabled = 0;
        g_world_shop_background_visible = 0;
        g_world_shop_equip_candidate_item = g_item_list_entries[g_world_menu_cursor_position];
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_equip_candidate_preview_visible = 0;
        g_world_shop_equip_candidate_initialized = 1;
    }
    g_world_shop_equip_candidate_saved_browse = g_world_formation_unit_browse_enabled;
    world_formation_update_and_draw_unit_grid(0, g_world_formation_unit_browse_enabled == 0, 1, input,
        g_menu_input_lock_mode, world_item_get_equip_candidate_marker, world_item_get_equip_candidate_brightness);
    if (g_world_shop_equip_candidate_saved_browse == 0) {
        if (g_world_formation_unit_browse_enabled != 0) {
            g_world_shop_equip_candidate_last_unit = -1;
            return;
        }
    } else if (g_world_formation_unit_browse_enabled == 0) {
        g_menu_status_display_thread_params.redraw_request = 1;
    }
    if (g_world_formation_unit_banner_enabled != 0 && g_world_formation_unit_browse_enabled == 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_world_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_world_menu_description_text_id = text_id;
    } else {
        g_world_menu_description_text_id = 0;
    }
    if (g_world_formation_unit_browse_enabled != 0) {
        if (g_unit_selected_index != g_world_shop_equip_candidate_last_unit) {
            world_formation_stage_selected_unit();
            i = 0;
            equipped = 0;
            g_world_shop_equip_candidate_last_unit = g_unit_selected_index;
            for (; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                if (g_world_unit_pointers[g_unit_selected_index]->equipment[i] == g_world_shop_equip_candidate_item) {
                    equipped = 1;
                    g_world_selected_unit_stat_detail.state.equipment[i] |= 0x8000;
                }
            }
            if (equipped != 0
                || world_unit_can_equip_item_id(g_unit_selected_index, g_world_shop_equip_candidate_item) != 1) {
                g_world_shop_equip_candidate_preview_visible = 0;
            } else {
                slot = world_item_select_equipment_slot(
                    WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_world_shop_equip_candidate_item);
                world_formation_equip_item_to_unit_slot(
                    WORLD_FORMATION_PREVIEW_UNIT_INDEX, slot, g_world_shop_equip_candidate_item);
                bcopy(g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]->equipment,
                    g_world_item_preview_stat_detail.state.equipment, 10);
                item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]->equipment);
                g_world_item_preview_stat_detail.state.two_hands
                    = unit_is_two_handing_weapon(g_world_item_preview_stat_detail.state.equipment,
                        world_ability_has_two_hands(g_unit_selected_index));
                g_world_shop_equip_candidate_preview_visible = 1;
                g_world_item_preview_stat_detail.state.equipment[slot]
                    = g_world_shop_equip_candidate_item | WORLD_EQUIPMENT_SHOP_ITEM;
            }
        }
    } else {
        g_world_shop_equip_candidate_preview_visible = 0;
        if (g_world_shop_equip_candidate_saved_browse == 0 && (input & PSX_PAD_CROSS)) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
            menu_set_vsync_mode_and_event_speed(2);
            g_world_shop_background_visible = 1;
            g_world_shop_equip_candidate_initialized = 0;
        }
    }
    world_menu_toggle_preview_stats_window(g_world_shop_equip_candidate_preview_visible);
    world_menu_toggle_stat_preview_panel_thread(g_world_shop_equip_candidate_preview_visible);
}
