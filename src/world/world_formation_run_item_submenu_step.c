#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Formation item-menu supervisor: menu 9 of world_formation_item_menu's
 * switch, which calls this as `done = world_formation_run_item_submenu_step()`. Dispatches to the
 * category / rearrange / equip-preview sub-menus and redraws the row window.
 *
 * `busy = 0;` is written after the pressable-button call. The target
 * interleaves `move s0,zero` into that call's argument setup (after
 * `li a2,0x3c`, before `lui a3`); written before the call it is instead
 * emitted first, and reorg then steals it into the preceding `bnez` delay slot
 * and retargets the branch past its own slot instruction. `busy` is not an
 * argument to that call -- it is only read by the script call below -- so the
 * later position is the natural one.
 *
 * fft/world.h owns g_world_formation_unit_cycle_mode,
 * g_world_formation_cursor_ot_override, g_world_preview_stats_thread_params, g_world_preview_stats_thread_params.y,
 * g_world_item_submenu_initialized, g_world_menu_* and g_world_formation_unit_banner_enabled, so those are not
 * redeclared here.
 *
 * g_world_item_submenu_initialized is `u8` in the header while the target reads it with `lb`, so it is reached
 * through an lvalue reinterpretation, as the twin src/world/world_item_run_category_menu.c does for the
 * sibling byte g_world_item_category_menu_initialized (`*(s8*)&g_world_item_category_menu_initialized`).
 *
 * 0x801c8518 carries two catalogued names and the store and the reload use
 * different ones, matching that same twin: one symbol for both lets cse
 * forward the stored value. */

s32 world_formation_run_item_submenu_step(void) {
    s32 input;
    s32 disabled;
    s32 busy;
    s32 unused[6]; /* never referenced; reproduces the 0x48 frame */

    if (*(s8*)&g_world_item_submenu_initialized == 0) {
        g_world_item_submenu_initialized = 1;
        g_world_item_category_view = world_menu_set_cursor_position_2(7, 0);
        world_thread_suspend(0xF);
        world_menu_stop_unit_status_banner_thread(8);
        world_menu_stop_unit_status_banner_thread(0xC);
        world_menu_stop_unit_status_banner_thread(0xA);
        g_world_item_menu_mode = 0;
        g_world_formation_unit_banner_enabled = 0;
        g_world_preview_stats_thread_params.x = 0;
        g_world_preview_stats_thread_params.y = -0x49;
    }
    g_world_formation_cursor_ot_override = 9;
    if (g_world_item_menu_mode == -1) {
        world_menu_toggle_preview_stats_window(0);
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_item_submenu_initialized = 0;
        g_world_formation_unit_banner_enabled = 1;
        world_thread_resume(0xF);
        g_world_preview_stats_thread_params.x = 2;
        g_world_preview_stats_thread_params.y = 2;
        g_world_formation_unit_cycle_mode = 2;
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_world_item_menu_mode == 0) {
        g_world_item_menu_mode = world_item_run_category_menu();
    } else if (g_world_item_menu_mode == 1) {
        g_world_item_menu_mode = world_item_run_rearrange_mode();
    } else if (g_world_item_menu_mode == 2) {
        g_world_formation_cursor_ot_override = 0x3C;
        g_world_item_menu_mode = world_item_run_action_menu();
    } else if (g_world_item_menu_mode == 5) {
        g_world_item_menu_mode = world_item_run_sort_order_menu();
    } else if (g_world_item_menu_mode == 6) {
        g_world_item_menu_mode = world_item_show_units_equipping_selected();
        return 1;
    }
    if (g_world_item_menu_mode == 6) {
        return 1;
    }
    input = 0;
    if (g_world_item_menu_mode < 2) {
        input = g_world_input_primary_repeat;
        g_world_menu_prompt_active = 0;
    } else {
        g_world_menu_prompt_active = 1;
    }
    disabled = 0;
    if (world_map_is_busy() != 0 || g_world_item_menu_mode != 0
        || world_count_item_equipped_by_party(g_world_menu_entry_ids[g_world_menu_cursor_position]) == 0) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(2, 0xB8, 0x3C, (u16)g_world_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, 1);
    busy = 0;
    /* The target passes a0 = 0 to the argument-less world_thread_is_task_active. */
    if (((s32 (*)(s32))world_thread_is_task_active)(0) != 0 || world_thread_is_running(5) != 0) {
        busy = 1;
    }
    world_menu_run_script_with_palette_mode(g_world_item_list_window_script, input, busy);
    world_menu_draw_row_window_and_cursor(0, g_world_item_menu_category, busy);
    if (g_world_menu_option_count != 0) {
        if (g_world_menu_entry_ids[g_world_menu_cursor_position] < 0x7A) {
            world_item_draw_weapon_hand_icons(g_world_menu_entry_ids[g_world_menu_cursor_position]);
        }
        if ((u16)g_world_menu_entry_ids[g_world_menu_cursor_position] - 0x90 < 0x40U) {
            world_menu_run_script_with_palette_mode(g_world_item_hp_mp_bonus_window_script, 0, 0);
        }
        world_item_build_status_list_polygons(g_world_menu_entry_ids[g_world_menu_cursor_position]);
    }
    return 1;
}
