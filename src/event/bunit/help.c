/* BUNIT's menu thread and field-help cursor open text for the selected unit
 * while its normal menu input waits. */

#include "fft/help.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/message.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "psx/pad.h"
#include "psx/types.h"

/* menu thread */
extern u8 g_unit_list_thread_running;

/* help cursor */
extern help_navigation_record_t* g_unit_list_help_nodes;
extern help_navigation_record_t g_unit_list_help_nodes_unit_browse[];
extern help_navigation_record_t g_unit_list_help_nodes_banner_top[];
extern help_navigation_record_t g_unit_list_help_nodes_banner_bottom[];
extern u8 g_unit_list_help_node_index;
extern u16 g_unit_list_help_cursor_anim_state;
extern u8 g_unit_list_help_cursor_initialized;

/* Aliases of g_unit_list_help_nodes/g_unit_list_help_node_index; the target reloads them at each use
 * instead of reusing the cached copies, which separate symbols reproduce. */
extern help_navigation_record_t* g_unit_list_help_nodes_reload;
extern u8 g_unit_list_help_node_index_reload;
extern u8 g_unit_list_help_node_index_reload_2;

/* g_unit_list_help_nodes_unit_browse[23].help_base and [24].help_base as their own symbols: as element
 * accesses GCC derives the second store and the later table address from one base register. */
extern s16 g_unit_list_help_node_23_help_base;
extern s16 g_unit_list_help_node_24_help_base;

/* Run menu `desc` on thread slot `id`: on the first call (while
 * g_unit_list_thread_running is clear) publish it as g_menu_thread_menu_data and start
 * its thread there, unless the slot is busy. Returns 1 while that thread runs and 0 once
 * it has finished, also kept in g_unit_list_thread_running. */
s32 unit_list_update_menu_thread(s32 id, menu_entry_t* desc) {
    if (g_unit_list_thread_running == 0) {
        if (thread_is_running(id) != 0) {
            return 1;
        }
        g_menu_thread_menu_data = desc;
        thread_start(id, desc->thread_entry);
        thread_parameters_set(id, (s32)g_menu_thread_menu_data, 0, 0);
        g_unit_list_thread_running = 1;
    }
    g_unit_list_thread_running = thread_is_running(id);
    return g_unit_list_thread_running;
}

/* Start the text thread for g_text_selection_id (ids above 0x1FFFF are
 * stashed in g_unit_list_help_screen_id instead); thread_data + 0x38 is the text
 * thread's dialog type, as in menu_text_start_help_thread. */
void unit_list_start_text_thread(s32 thread_data) {
    if (g_text_selection_id > 0x1FFFF) {
        g_unit_list_help_screen_id = g_text_selection_id;
        return;
    }
    if (g_text_selection_id <= 0) {
        return;
    }
    text_section_pointers_save();
    text_pointer_table_relocate(g_help_text_table);
    thread_start(THREAD_SCREEN_SLOT_TEXT, message_dialog_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_TEXT, thread_data + 0x38, g_text_selection_id, 0);
    g_menu_input_lock_mode = 1;
}

/* The unit list's field-help cursor. The first call picks the node table for the help
 * screen in g_unit_list_help_screen_id; then, while no help text is up, the D-pad follows
 * the node links, CIRCLE opens the help text for the node's field and CROSS closes the
 * help screen. The cursor is drawn on the selected node every call. `mode` is the unit's
 * uses_monster_skillset flag.
 *
 * The target reloads the active table and node index during confirmation.
 * Separate linker aliases preserve those loads; volatile accesses change the
 * instruction schedule. */
void unit_list_update_help_cursor(s16 mode) {
    point16_t pos;
    u32 flags;
    help_navigation_record_t* node;
    u16 x;
    u16 y;
    s32 running;
    s32 text_id;
    s32 node_index;

    if (g_unit_list_help_cursor_initialized == 0) {
        /* Nodes 23 and 24 show skillset help (HELP_TEXT_ID_SKILLSET_BASE as a 16-bit id), or
         * ability help for a unit on a monster skillset. */
        if (mode == 0) {
            g_unit_list_help_node_23_help_base = (s16)HELP_TEXT_ID_SKILLSET_BASE;
            g_unit_list_help_node_24_help_base = (s16)HELP_TEXT_ID_SKILLSET_BASE;
        } else {
            g_unit_list_help_node_23_help_base = HELP_TEXT_ID_ABILITY_BASE;
            g_unit_list_help_node_24_help_base = HELP_TEXT_ID_ABILITY_BASE;
        }
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        g_unit_list_help_cursor_initialized = 1;
        g_unit_list_help_node_index = 0;
        if (g_unit_list_help_screen_id == MENU_GRID_HELP_BANNER_TOP) {
            g_unit_list_help_nodes = g_unit_list_help_nodes_banner_top;
        } else if (g_unit_list_help_screen_id == MENU_GRID_HELP_BANNER_BOTTOM) {
            g_unit_list_help_nodes = g_unit_list_help_nodes_banner_bottom;
        } else {
            g_unit_list_help_nodes = g_unit_list_help_nodes_unit_browse;
        }
    }
    running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
    if (running == 0) {
        flags = g_unit_list_input_primary_repeat;
        node_index = g_unit_list_help_node_index;
        if (flags & PSX_PAD_CROSS) {
            g_text_selection_id = g_unit_list_help_screen_id;
            g_unit_list_help_screen_id = MENU_GRID_HELP_NONE;
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_unit_list_help_cursor_initialized = 0;
        } else if (flags & PSX_PAD_CIRCLE) {
            text_id = 0;
            if (g_unit_list_help_nodes[node_index].value != 0) {
                text_id = g_unit_list_help_nodes[node_index].value[0];
            }
            if ((g_unit_list_help_node_index_reload - 0x12) >= 0xAU || text_id != 0) {
                /* The target zero-extends help_base here (lhu): a -0x6800 base adds HELP_TEXT_ID_SKILLSET_BASE. */
                g_text_selection_id
                    = text_id + (u16)g_unit_list_help_nodes_reload[g_unit_list_help_node_index_reload_2].help_base;
                unit_list_start_text_thread(3);
            }
        } else if (flags & PSX_PAD_UP) {
            node_index = g_unit_list_help_nodes[node_index].destination[0];
        } else if (flags & PSX_PAD_DOWN) {
            node_index = g_unit_list_help_nodes[node_index].destination[1];
        } else if (flags & PSX_PAD_RIGHT) {
            node_index = g_unit_list_help_nodes[node_index].destination[2];
        } else if (flags & PSX_PAD_LEFT) {
            node_index = g_unit_list_help_nodes[node_index].destination[3];
        }
        /* Off the player's team, node 6 is skipped: 2 and 7 lead to each other. */
        if (g_unit_panel_selected_unit_status.team_kind != 0) {
            if (g_unit_list_help_node_index == 2 && node_index == 6) {
                node_index = 7;
            }
            if (g_unit_list_help_node_index == 7 && node_index == 6) {
                node_index = 2;
            }
        }
        if (node_index != g_unit_list_help_node_index) {
            g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
            g_unit_list_help_node_index = node_index;
        }
    }
    node = &g_unit_list_help_nodes[g_unit_list_help_node_index];
    x = node->x;
    pos.x = x;
    y = node->y;
    pos.y = y;
    if (node->vertical_cursor != 0) {
        pos.y = y + 1;
        menu_update_and_draw_bouncing_cursor(&pos, &g_unit_list_help_cursor_anim_state, running);
    } else {
        pos.x = x - 2;
        pos.y = y + 6;
        menu_update_and_draw_sliding_cursor(&pos, &g_unit_list_help_cursor_anim_state, running);
    }
}
