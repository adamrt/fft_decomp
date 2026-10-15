#include "fft/event_bunit.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Select the menu's node table, handle directional/confirm/cancel input, and
 * draw the selected node's cursor.
 *
 * The target reloads the active table and node index during confirmation.
 * Separate linker aliases preserve those loads; volatile accesses change the
 * instruction schedule. */
void bunit_menu_update_node_cursor(s16 mode) {
    point16_t pos;
    u32 flags;
    help_navigation_record_t* node;
    u16 x;
    u16 y;
    s32 running;
    s32 text_id;
    s32 node_index;

    if (g_bunit_help_cursor_initialized == 0) {
        /* Monster-ability help bases for the two ability nodes. */
        if (mode == 0) {
            g_bunit_help_node_23_help_base = -0x6800;
            g_bunit_help_node_24_help_base = -0x6800;
        } else {
            g_bunit_help_node_23_help_base = 0x7800;
            g_bunit_help_node_24_help_base = 0x7800;
        }
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_bunit_help_cursor_initialized = 1;
        g_bunit_help_node_index = 0;
        if (g_bunit_help_screen_id == MENU_GRID_HELP_BANNER_TOP) {
            g_bunit_help_nodes = g_bunit_help_nodes_banner_top;
        } else if (g_bunit_help_screen_id == MENU_GRID_HELP_BANNER_BOTTOM) {
            g_bunit_help_nodes = g_bunit_help_nodes_banner_bottom;
        } else {
            g_bunit_help_nodes = g_bunit_help_nodes_unit_browse;
        }
    }
    running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
    if (running == 0) {
        flags = g_bunit_input_primary_repeat;
        node_index = g_bunit_help_node_index;
        if (flags & PSX_PAD_CROSS) {
            g_text_selection_id = g_bunit_help_screen_id;
            g_bunit_help_screen_id = MENU_GRID_HELP_NONE;
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_bunit_help_cursor_initialized = 0;
        } else if (flags & PSX_PAD_CIRCLE) {
            text_id = 0;
            if (g_bunit_help_nodes[node_index].value != 0) {
                text_id = g_bunit_help_nodes[node_index].value[0];
            }
            if ((g_bunit_help_node_index_reload - 0x12) >= 0xAU || text_id != 0) {
                /* The target zero-extends help_base here (lhu): a -0x6800 base adds 0x9800. */
                g_text_selection_id
                    = text_id + (u16)g_bunit_help_nodes_reload[g_bunit_help_node_index_reload_2].help_base;
                bunit_text_start_selection_thread(3);
            }
        } else if (flags & PSX_PAD_UP) {
            node_index = g_bunit_help_nodes[node_index].destination[0];
        } else if (flags & PSX_PAD_DOWN) {
            node_index = g_bunit_help_nodes[node_index].destination[1];
        } else if (flags & PSX_PAD_RIGHT) {
            node_index = g_bunit_help_nodes[node_index].destination[2];
        } else if (flags & PSX_PAD_LEFT) {
            node_index = g_bunit_help_nodes[node_index].destination[3];
        }
        if (g_panel_selected_billboard.team_kind != 0) {
            if (g_bunit_help_node_index == 2 && node_index == 6) {
                node_index = 7;
            }
            if (g_bunit_help_node_index == 7 && node_index == 6) {
                node_index = 2;
            }
        }
        if (node_index != g_bunit_help_node_index) {
            g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
            g_bunit_help_node_index = node_index;
        }
    }
    node = &g_bunit_help_nodes[g_bunit_help_node_index];
    x = node->x;
    pos.x = x;
    y = node->y;
    pos.y = y;
    if (node->vertical_cursor != 0) {
        pos.y = y + 1;
        menu_update_and_draw_bouncing_cursor_variant(&pos, &g_bunit_help_cursor_anim_state, running);
    } else {
        pos.x = x - 2;
        pos.y = y + 6;
        menu_update_and_draw_sliding_cursor_variant(&pos, &g_bunit_help_cursor_anim_state, running);
    }
}
