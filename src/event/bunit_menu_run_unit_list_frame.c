/* Unit-list menu frame handler (g_bunit_menu_current_menu state -1 / 0). */
#include "fft/event_bunit.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 bunit_menu_run_unit_list_frame(void) {
    s32 input;
    bunit_unit_data_t* unit;
    u8 mode_b3;
    u8 mode_b4;

    input = g_bunit_menu_input_active_mask;
    if (g_bunit_menu_current_menu == -1) {
        if (g_bunit_unit_list_initialized == 0) {
            g_bunit_status_banner_enabled = g_bunit_saved_status_banner_enabled;
            g_bunit_unit_browse_enabled = g_bunit_saved_unit_browse_enabled;
            /* The target passes the selected index to this argument-less callee. */
            ((void (*)(s32))bunit_menu_init_unit_list)(g_bunit_unit_selected_index);
            g_bunit_unit_list_count = bunit_panel_build_unit_billboard_list(-1, (s32)g_bunit_unit_data, 1);
            g_bunit_unit_reorderable_count = bunit_unit_build_reorderable_index_list();
            bunit_unit_build_list_index_map();
            g_bunit_unit_grid_selected_cell = g_bunit_unit_list_index_map[g_bunit_unit_selected_index];
            bunit_unit_copy_selected_data();
            g_bunit_status_display_highlight = 0;
            g_bunit_numeric_editor_highlight = 0;
            g_bunit_character_status_highlight = 0;
            bunit_menu_reset_results();
            g_bunit_unit_list_initialized = 1;
        }
        if (bunit_gfx_update_increasing_fade() == 0) {
            g_bunit_unit_list_initialized = 0;
            bunit_thread_request_stop(5);
            return 0;
        }
        if (g_bunit_unit_list_triangle_menu_open != 0) {
            g_bunit_text_selection_id = g_bunit_unit_list_triangle_menu.selected_index != 0 ? 0x1001 : 0x1000;
            if ((g_bunit_unit_list_triangle_menu_open
                    = bunit_menu_run_descriptor_thread(5, &g_bunit_unit_list_triangle_menu))
                == 0) {
                if (g_bunit_menu_primary_result == 0) {
                    mode_b3 = g_bunit_status_banner_enabled;
                    mode_b4 = g_bunit_unit_browse_enabled;
                    g_bunit_menu_current_menu = 0;
                    g_bunit_status_banner_enabled = 0;
                    g_bunit_unit_browse_enabled = 0;
                    g_bunit_saved_status_banner_enabled = mode_b3;
                    g_bunit_saved_unit_browse_enabled = mode_b4;
                    bunit_menu_reset_results();
                    g_bunit_unit_list_initialized = 0;
                } else if (g_bunit_menu_primary_result == 1) {
                    bunit_gfx_start_increasing_fade();
                    g_bunit_selected_roster_id = g_bunit_unit_data[g_bunit_unit_selected_index]->roster_id;
                }
                g_bunit_character_status_highlight = 0;
                g_bunit_numeric_editor_highlight = 0;
                return 1;
            }
            return 1;
        }
        if (g_bunit_unit_browse_enabled != 0) {
            g_bunit_text_selection_id = 0x20002;
        } else if (g_bunit_status_banner_enabled != 0) {
            g_bunit_text_selection_id = g_bunit_status_banner_at_bottom != 0 ? 0x20001 : 0x20000;
        } else {
            g_bunit_text_selection_id = 0;
        }
        if (bunit_input_get_lock_timer() != 0) {
            return 1;
        }
        if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START)) {
            g_bunit_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_bunit_character_status_highlight = 1;
            g_bunit_numeric_editor_highlight = 1;
            g_bunit_unit_list_triangle_menu_open = 1;
        } else if ((input & PSX_PAD_CIRCLE) && g_bunit_unit_browse_enabled != 0) {
            unit = g_bunit_unit_data[g_bunit_unit_selected_index];
            if (unit->uses_monster_skillset != 0 || (unit->initial_team_flags & 0x30)) {
                g_bunit_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                g_bunit_status_display_highlight = 1;
                g_bunit_numeric_editor_highlight = 1;
                g_bunit_character_status_highlight = 1;
                g_bunit_unit_list_initialized = 0;
                g_bunit_saved_unit_browse_enabled = g_bunit_unit_browse_enabled;
                g_bunit_menu_current_menu = 2;
                g_bunit_saved_status_banner_enabled = g_bunit_status_banner_enabled;
            }
        } else if (g_bunit_unit_browse_enabled == 0 && (input & PSX_PAD_CROSS)) {
            bunit_gfx_start_increasing_fade();
        }
    } else if (g_bunit_menu_current_menu == 0) {
        bunit_menu_run_reorder_list();
    }
    return 1;
}
