#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

extern world_order_menu_entry_t g_world_formation_panel_windows[];

extern void world_formation_update_and_draw_unit_grid(s32, s32, s32, s32, s32, s32 (*)(s32), s32 (*)(s32));
typedef struct weapon_pair weapon_pair_t;
extern s32 world_item_check_two_hands_for_weapons(weapon_pair_t* slots, s32 two_hands_support);

/*
 * Run one frame of the shop's "who can equip" unit browser for the item
 * under the list cursor.
 *
 * Moving onto a unit that can equip the item stages it in the temporary
 * formation record (pointer slot 20) and shows the stat preview; slots
 * already holding the item are flagged in g_world_selected_unit_stat_detail.equipment. Cancel returns to shop
 * step 1.
 */
void world_shop_run_equip_candidate_step(void) {
    s32 unused[16];
    s32 input;
    u8 equipped;
    s32 i;
    s32 slot;
    s32 text_id;

    input = g_world_input_secondary_repeat;
    if (g_world_shop_equip_candidate_initialized == 0) {
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_formation_unit_browse_enabled = 0;
        g_world_shop_background_visible = 0;
        g_world_shop_equip_candidate_item = g_world_menu_entry_ids[g_world_menu_cursor_position];
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_equip_candidate_preview_visible = 0;
        g_world_shop_equip_candidate_initialized = 1;
    }
    g_world_shop_equip_candidate_saved_browse = g_world_formation_unit_browse_enabled;
    world_formation_update_and_draw_unit_grid(0, g_world_formation_unit_browse_enabled == 0, 1, (s16)input,
        *(s16*)&g_world_thread_task_active, world_item_get_equip_candidate_marker,
        world_item_get_equip_candidate_brightness);
    if (g_world_shop_equip_candidate_saved_browse == 0) {
        if (g_world_formation_unit_browse_enabled != 0) {
            g_world_shop_equip_candidate_last_unit = -1;
            return;
        }
    } else if (g_world_formation_unit_browse_enabled == 0) {
        g_world_formation_panel_windows[0].enabled = 1;
    }
    if (g_world_formation_unit_banner_enabled != 0 && g_world_formation_unit_browse_enabled == 0) {
        text_id = 0x20000;
        if (g_world_formation_scroll_enabled != 0) {
            text_id = 0x20001;
        }
        g_world_menu_description_text_id = text_id;
    } else {
        g_world_menu_description_text_id = 0;
    }
    if (g_world_formation_unit_browse_enabled != 0) {
        if (g_world_formation_selected_unit_index != g_world_shop_equip_candidate_last_unit) {
            world_formation_stage_selected_unit();
            i = 0;
            equipped = 0;
            g_world_shop_equip_candidate_last_unit = g_world_formation_selected_unit_index;
            for (; i < 5; i++) {
                if (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[i]
                    == g_world_shop_equip_candidate_item) {
                    equipped = 1;
                    g_world_selected_unit_stat_detail.equipment[i] |= 0x8000;
                }
            }
            if (equipped != 0
                || world_unit_can_equip_item_id(
                       g_world_formation_selected_unit_index, g_world_shop_equip_candidate_item)
                    != 1) {
                g_world_shop_equip_candidate_preview_visible = 0;
            } else {
                slot = world_item_select_equipment_slot(20, g_world_shop_equip_candidate_item);
                world_formation_equip_item_to_unit_slot(20, slot, g_world_shop_equip_candidate_item);
                bcopy(g_world_formation_unit_pointers[20]->equipment, g_world_item_preview_stat_detail.equipment, 10);
                world_item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
                    &g_world_selected_unit_stat_summary,
                    g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                    g_world_formation_unit_pointers[20]->equipment);
                g_world_item_preview_stat_detail.two_hands
                    = world_item_check_two_hands_for_weapons((weapon_pair_t*)g_world_item_preview_stat_detail.equipment,
                        world_ability_has_two_hands(g_world_formation_selected_unit_index));
                g_world_shop_equip_candidate_preview_visible = 1;
                g_world_item_preview_stat_detail.equipment[slot] = g_world_shop_equip_candidate_item | 0x8000;
            }
        }
    } else {
        g_world_shop_equip_candidate_preview_visible = 0;
        if (g_world_shop_equip_candidate_saved_browse == 0 && (input & PSX_PAD_CROSS)) {
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = 1;
            world_script_set_vsync_mode_and_event_speed(2);
            g_world_shop_background_visible = 1;
            g_world_shop_equip_candidate_initialized = 0;
        }
    }
    world_menu_toggle_preview_stats_window(g_world_shop_equip_candidate_preview_visible);
    world_menu_toggle_stat_preview_panel_thread(g_world_shop_equip_candidate_preview_visible);
}
