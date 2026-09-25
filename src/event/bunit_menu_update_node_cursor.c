#include "fft/event_bunit.h"
#include "psx/types.h"

/* Select the menu's node table, handle directional/confirm/cancel input, and
 * draw the selected node's cursor.
 *
 * The target reloads the active table and node index during confirmation.
 * Separate linker aliases preserve those loads; volatile accesses change the
 * instruction schedule. */
void bunit_menu_update_node_cursor(s16 mode) {
    u16 pos[2];
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
        g_bunit_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_bunit_help_cursor_initialized = 1;
        g_bunit_help_node_index = 0;
        if (g_bunit_help_screen_id == 0x20000) {
            g_bunit_help_nodes = g_bunit_help_nodes_banner_top;
        } else if (g_bunit_help_screen_id == 0x20001) {
            g_bunit_help_nodes = g_bunit_help_nodes_banner_bottom;
        } else {
            g_bunit_help_nodes = g_bunit_help_nodes_unit_browse;
        }
    }
    running = battle_thread_is_running(1);
    if (running == 0) {
        flags = g_bunit_input_primary_repeat;
        node_index = g_bunit_help_node_index;
        if (flags & 0x40) {
            g_bunit_text_selection_id = g_bunit_help_screen_id;
            g_bunit_help_screen_id = 0;
            g_bunit_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_bunit_help_cursor_initialized = 0;
        } else if (flags & 0x20) {
            text_id = 0;
            if (g_bunit_help_nodes[node_index].value != 0) {
                text_id = g_bunit_help_nodes[node_index].value[0];
            }
            if ((u32)(g_bunit_help_node_index_reload - 0x12) >= 0xAU || text_id != 0) {
                /* The target zero-extends help_base here (lhu): a -0x6800 base adds 0x9800. */
                g_bunit_text_selection_id
                    = text_id + (u16)g_bunit_help_nodes_reload[g_bunit_help_node_index_reload_2].help_base;
                /* This call site supplies literal 3 and leaves the text ID in
                 * a1; preserve that target ABI despite the callee's pointer
                 * interface. */
                ((void (*)(s32, s32))bunit_text_start_selection_thread)(3, text_id);
            }
        } else if (flags & 0x1000) {
            node_index = g_bunit_help_nodes[node_index].destination[0];
        } else if (flags & 0x4000) {
            node_index = g_bunit_help_nodes[node_index].destination[1];
        } else if (flags & 0x2000) {
            node_index = g_bunit_help_nodes[node_index].destination[2];
        } else if (flags & 0x8000) {
            node_index = g_bunit_help_nodes[node_index].destination[3];
        }
        if (g_bunit_panel_selected_billboard.mode != 0) {
            if (g_bunit_help_node_index == 2 && node_index == 6) {
                node_index = 7;
            }
            if (g_bunit_help_node_index == 7 && node_index == 6) {
                node_index = 2;
            }
        }
        if (node_index != g_bunit_help_node_index) {
            g_bunit_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
            g_bunit_help_node_index = node_index;
        }
    }
    node = &g_bunit_help_nodes[g_bunit_help_node_index];
    x = node->x;
    pos[0] = x;
    y = node->y;
    pos[1] = y;
    if (node->vertical_cursor != 0) {
        pos[1] = y + 1;
        bunit_menu_update_and_draw_bouncing_cursor(pos, g_bunit_help_cursor_anim_state, running);
    } else {
        pos[0] = x - 2;
        pos[1] = y + 6;
        bunit_menu_update_and_draw_animated_cursor(pos, g_bunit_help_cursor_anim_state, running);
    }
}
