/* Yield-then-stop, crystal/treasure result threads and AT list build/select threads. */

#include "fft/at_list.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/pad.h"
#include "psx/types.h"

/* crystal result menu */
/* Seven menu entries form the crystal/treasure result panel. The AT-list
 * and confirmation groups each contain a message entry followed by a choice. */
typedef enum crystal_pickup_entry {
    MENU_CRYSTAL_PICKUP_LIST = 0,
    MENU_CRYSTAL_PICKUP_AT_LIST = 1,
    MENU_CRYSTAL_PICKUP_CONFIRM = 3,
    MENU_CRYSTAL_PICKUP_MESSAGE = 5,
    MENU_CRYSTAL_PICKUP_CHOICE = 6,
    MENU_CRYSTAL_PICKUP_ENTRY_COUNT = 7
} crystal_pickup_entry_e;

extern menu_entry_t g_crystal_pickup_menu[MENU_CRYSTAL_PICKUP_ENTRY_COUNT];
extern menu_text_layout_t g_crystal_pickup_list_layout;
extern s16 g_crystal_pickup_row_colors[];
extern u32* g_crystal_pickup_controller_input;
extern s16 g_crystal_pickup_menu_result;
extern s16 g_crystal_pickup_choice_result;

/* AT list menu */
extern at_entry_t g_at_list_descriptors[AT_LIST_CAPACITY];
extern menu_entry_t g_at_list_state;

/* WORLD and OPTION each define one column layout for the AT List menu. */
extern menu_text_layout_t g_at_list_entry_table;
extern u8 g_at_list_text[];
extern s16 g_at_list_indices[];
extern s16 g_at_list_primary_text_ids[];
extern s16 g_at_list_secondary_text_ids[];
extern s16 g_at_list_row_text_colors[];

/* Give other event work one turn before terminating this thread. */
void thread_stop_after_yield(void) {
    thread_yield();
    thread_exit_current();
}

/* Run the menus of a crystal or treasure pickup (g_crystal_pickup_context) and leave the
 * outcome in g_crystal_pickup_action: CRYSTAL_PICKUP_NOTHING shows the nothing-found message
 * (action 0), TREASURE names the treasure item, sold for a quarter of its price at the 99 limit
 * (action 4). A crystal offers its learned-ability list (LEARNED, action 1 when one is taken),
 * the confirm menu (CRYSTAL, action 2) or a choice between the two (CRYSTAL | LEARNED). The
 * previous menu table is restored at the end. */
void crystal_start_pickup_threads(void) {
    crystal_pickup_result_t* context = g_crystal_pickup_context;
    menu_entry_t* previous_menu_descriptor = g_menu_thread_menu_data;
    menu_entry_t* menu;
    s32 value_count;
    s32 byte_index;
    s32 bit_index;
    s32 index;
    s16 message_id;

    /* Publish the menu before its local copy and selector store.
     * Reordering them changes address CSE. */
    g_menu_thread_menu_data = g_crystal_pickup_menu;
    menu = g_crystal_pickup_menu;
    g_crystal_pickup_menu[MENU_CRYSTAL_PICKUP_LIST].select_text_table = 5;
    g_crystal_pickup_menu[MENU_CRYSTAL_PICKUP_LIST].context = &g_crystal_pickup_list_layout;

    for (index = 0x1ff; index >= 0; index--) {
        g_crystal_pickup_row_colors[index] = 0;
    }

    /* List the ability text ids whose bits are set in the 0x40-byte learned set. */
    value_count = 0;
    for (byte_index = 0; byte_index < 0x40; byte_index++) {
        s32 mask = 1;
        /* A pointer local, not `context->learned_abilities[byte_index]` in the test: the
         * indexed form folds 0x53 into the load displacement, while the
         * target biases the index and loads at 0(reg). */
        u8* flag_byte = &context->learned_abilities[byte_index];

        for (bit_index = 0; bit_index < 8; bit_index++) {
            if ((*flag_byte & mask) != 0) {
                /* Separate statement: written as one expression, fold
                 * reassociates the ability-name base with `byte_index * 8` and loop.c turns
                 * the pair into a strength-reduced giv. */
                s32 value = bit_index + TEXT_ID_ABILITY_NAME_BASE;

                g_at_list_primary_text_ids[value_count++] = byte_index * 8 + value;
            }
            mask *= 2;
        }
    }

    menu->window.x = 0xc8;
    menu->window.y = 0x40;
    menu->window.w = 0x70;
    menu->text_image.source.w = 0x70;
    menu->inner_width = 0x70;
    if (value_count < 8) {
        menu_text_layout_t* layout;
        layout = menu->context;
        layout->row_count = value_count;
        layout = menu->context;
        layout->hidden_rows = 0;
    } else {
        menu_text_layout_t* layout;
        layout = menu->context;
        layout->row_count = 8;
        layout = menu->context;
        layout->hidden_rows = value_count - 8;
    }

    /* Nothing to pick up: show message 0x183c and end. */
    if (context->result == CRYSTAL_PICKUP_NOTHING) {
        message_id = 0x183c;
        g_menu_thread_menu_data[MENU_CRYSTAL_PICKUP_MESSAGE].text_id = message_id;
        thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
        thread_set_parameters(
            g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[MENU_CRYSTAL_PICKUP_MESSAGE], 0, 0);
        thread_wait_until_inactive(g_thread_current_id - 1);
        thread_wait_frames(2);
        g_crystal_pickup_action = 0;
        g_menu_thread_menu_data = previous_menu_descriptor;
        g_command_pending_selection[0] = 0;
        /* Thread exit does not return; a source return changes epilogue sharing. */
        thread_exit_current();
    }

    g_crystal_pickup_item = 0;
    if (context->result == CRYSTAL_PICKUP_TREASURE) {
        /* At the 99 limit, message 0x101b also shows a quarter of the item's price. */
        if (item_get_total_quantity(context->treasure_item, 1) == 0x63) {
            g_text_substitution_values[0] = context->treasure_item;
            g_text_substitution_values[1] = context->treasure_item;
            message_id = 0x101b;
            g_text_substitution_values[2] = item_get_data(context->treasure_item)->price >> 2;
            g_crystal_pickup_item = context->treasure_item;
        } else {
            message_id = 0x101a;
            g_text_substitution_values[0] = context->treasure_item;
            g_crystal_pickup_item = context->treasure_item;
        }
        g_menu_thread_menu_data[MENU_CRYSTAL_PICKUP_MESSAGE].text_id = message_id;
        thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
        thread_set_parameters(
            g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[MENU_CRYSTAL_PICKUP_MESSAGE], 0, 0);
        thread_wait_until_inactive(g_thread_current_id - 1);
        thread_wait_frames(2);
        g_crystal_pickup_action = 4;
        g_menu_thread_menu_data = previous_menu_descriptor;
        g_command_pending_selection[0] = 0;
        thread_exit_current();
    }

    /* Close the tile, summary and two unit stats panels before the crystal menus. */

    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_UNIT_SUMMARY_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_RIGHT_UNIT_STATS, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_HOVERED_UNIT_STATS, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);

    /* LEARNED shows the learned-ability list, CRYSTAL the confirm menu, and both together first
     * offer the choice between them. */
    for (;;) {
        if (context->result == (CRYSTAL_PICKUP_CRYSTAL | CRYSTAL_PICKUP_LEARNED)) {
            g_crystal_pickup_choice_result = 0xff;
            thread_start(g_thread_current_id - 1, menu_run_icon_selection_loop);
            thread_set_parameters(
                g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[MENU_CRYSTAL_PICKUP_CHOICE], 0, 0);
            thread_wait_until_inactive(g_thread_current_id - 1);
        }

        g_crystal_pickup_menu_result = 0xff;
        if ((context->result == (CRYSTAL_PICKUP_CRYSTAL | CRYSTAL_PICKUP_LEARNED)
                && g_crystal_pickup_choice_result == 0)
            || context->result == CRYSTAL_PICKUP_LEARNED) {
            thread_start(g_thread_current_id - 2, at_list_run);
            thread_set_parameters(g_thread_current_id - 2, (s32)g_menu_thread_menu_data, 0, 0);
            thread_start(THREAD_BORROWED_SLOT_AT_LIST_MENU, menu_icon_linked_entry_thread);
            thread_set_parameters(
                THREAD_BORROWED_SLOT_AT_LIST_MENU, (s32)&g_menu_thread_menu_data[MENU_CRYSTAL_PICKUP_AT_LIST], 0, 0);
            for (;;) {
                thread_yield();
                if (thread_is_running_asm(g_thread_current_id - 3) != 0) {
                    thread_start(THREAD_BORROWED_SLOT_AT_LIST_MENU_CHILD, thread_wait_forever);
                } else {
                    thread_suspend(THREAD_BORROWED_SLOT_AT_LIST_MENU_CHILD);
                }
                if (thread_is_running_asm(g_thread_current_id - 1) == 0) {
                    break;
                }
                g_crystal_pickup_controller_input = script_get_controller_input_pointer(0);
                if ((*g_crystal_pickup_controller_input & PSX_PAD_SELECT) != 0) {
                    help_start_menu_thread();
                }
            }

            thread_wait_until_inactive(g_thread_current_id - 1);
            thread_set_parameters(THREAD_BORROWED_SLOT_AT_LIST_MENU, 0, 0, 1);
            thread_suspend(THREAD_BORROWED_SLOT_AT_LIST_MENU_CHILD);
            if (g_crystal_pickup_menu_result == 0) {
                g_crystal_pickup_action = 1;
                break;
            }
            /* Backing out of the list returns to the choice when both are offered. */
            if (g_crystal_pickup_menu_result == 1 || (g_crystal_pickup_menu_result & 0xff) == 0xff) {
                if (context->result == (CRYSTAL_PICKUP_CRYSTAL | CRYSTAL_PICKUP_LEARNED)) {
                    continue;
                }
            }
        }

        if ((g_crystal_pickup_choice_result == 1
                && context->result == (CRYSTAL_PICKUP_CRYSTAL | CRYSTAL_PICKUP_LEARNED))
            || context->result == CRYSTAL_PICKUP_CRYSTAL) {
            g_crystal_pickup_menu_result = 0xff;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(
                g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[MENU_CRYSTAL_PICKUP_CONFIRM], 0, 0);
            thread_wait_until_inactive(g_thread_current_id - 1);
            if (g_crystal_pickup_menu_result == 0) {
                g_crystal_pickup_action = 2;
                break;
            }
            if (context->result != 2) {
                continue;
            }
        }

        g_crystal_pickup_action = 0;
        break;
    }

    thread_wait_frames(0x14);
    g_menu_thread_menu_data = previous_menu_descriptor;
    g_command_pending_selection[0] = 0;
    thread_exit_current();
}

/*
 * Rebuild the AT (turn order) list from the descriptor table and size the
 * menu window around it.
 *
 * The target keeps two counters that always hold the same value: the loop
 * index addresses the descriptor table and the entry-number column, while
 * the separate accepted-entry counter addresses the text and color columns.
 * Merging them makes the four 16-bit columns share one induction variable
 * and loses the entry-number column's recomputed shift.
 *
 * The menu geometry is written per branch rather than through a shared
 * temporary: the store through the layout pointer may alias the menu record,
 * so the repeated window.y store is not redundant to the compiler.
 */
static void at_list_init(menu_entry_t* menu) {
    s32 entry_count;
    s32 has_special_entry;
    s32 index;
    u8* text;

    g_thread_call_target = (void (*)(void))at_list_build;
    thread_call_on_main_stack(g_at_list_descriptors, 0);

    for (index = 0; index < 0x100; index++) {
        g_crystal_pickup_row_colors[index] = 0;
        g_at_list_row_text_colors[index] = 0;
    }

    g_text_section_pointers[TEXT_SECTION_AT_LIST] = g_at_list_text;
    entry_count = 0;
    has_special_entry = 0;
    text = g_at_list_text;

    for (index = 0; index < AT_LIST_CAPACITY; index++) {
        unit_stats_t* unit;
        s32 name_index;

        if ((g_at_list_descriptors[index].unit_code & AT_UNIT_ID_MASK) == AT_END_OF_LIST) {
            break;
        }

        g_at_list_indices[index] = index + 1;
        unit = unit_get_stats(g_at_list_descriptors[index].unit_code & AT_UNIT_ID_MASK);
        if ((g_at_list_descriptors[index].unit_code & AT_ACTION_ENTRY) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbe;
        }
        /* The pointer must lead the index in the sum to match the target's
         * operand order on the address add. */
        for (name_index = 0; name_index < UNIT_NAME_SIZE; name_index++) {
            u8 character = unit->name[name_index];
            *text = character;
            if ((character & 0xfe) == 0xfe) {
                break;
            }
            text++;
        }
        if ((g_at_list_descriptors[index].unit_code & AT_ACTION_ENTRY) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbf;
        }
        *text++ = 0xfe;

        if ((g_at_list_descriptors[index].unit_code & AT_ACTION_ENTRY) != 0) {
            g_at_list_secondary_text_ids[entry_count] = index - 0x800;
        } else {
            g_at_list_primary_text_ids[entry_count] = index - 0x800;
        }

        if ((g_at_list_descriptors[index].unit_code & AT_ACTION_ENTRY) == 0) {
            g_at_list_secondary_text_ids[entry_count] = TEXT_ID_ABILITY_NAME_BASE;
        } else {
            has_special_entry = 1;
            if ((g_at_list_descriptors[index].unit_code & AT_JUMP) != 0) {
                g_at_list_primary_text_ids[entry_count] = 0xb012;
            } else {
                s32 value = g_at_list_descriptors[index].action_id_low;
                if ((g_at_list_descriptors[index].unit_code & AT_ABILITY_ID_HIGH_BIT) != 0) {
                    value += 0x100;
                }
                g_at_list_primary_text_ids[entry_count] = value + TEXT_ID_ABILITY_NAME_BASE;
            }
        }

        if ((unit->initial_team_flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE) {
            g_at_list_row_text_colors[entry_count] = 8;
        }
        entry_count++;
    }

    if (entry_count >= AT_LIST_CAPACITY) {
        entry_count = AT_LIST_CAPACITY - 1;
    }

    if (has_special_entry != 0) {
        menu_text_layout_t* layout;

        menu->window.y = 0x20;
        layout = menu->context;
        layout->row_count = 10;
        menu->window.x = 0x8c;
        menu->window.y = 0x20;
        menu->window.w = 0xe8;
        menu->text_image.source.w = 0xe8;
        menu->inner_width = 0xe8;
    } else {
        menu_text_layout_t* layout;

        menu->window.y = 0x20;
        layout = menu->context;
        layout->row_count = 10;
        menu->window.x = 0xbe;
        menu->window.y = 0x20;
        menu->window.w = 0x84;
        menu->text_image.source.w = 0x84;
        menu->inner_width = 0x84;
    }

    if (entry_count < 10) {
        menu_text_layout_t* layout;
        menu_text_layout_t* resized_layout;

        menu->window.y = (10 - entry_count) * 8 + 0x20;
        layout = menu->context;
        layout->row_count = entry_count;
        resized_layout = menu->context;
        resized_layout->hidden_rows = 0;
    } else {
        menu_text_layout_t* layout;

        layout = menu->context;
        layout->hidden_rows = entry_count - 10;
    }
}

/* Pause the four worker threads after this one and yield until
 * g_at_list_selection_finished is raised. Publish the AT list's cursor row plus
 * 100 in g_menu_transition_state before each completion check, including the final one.
 * Then reset the state, close the unit view panels (threads 13..10), rebind
 * TEXT_SECTION_AT_LIST to g_at_list_text, resume the workers and stop this thread. */
void at_list_select_unit_thread(void) {
    g_menu_modal_open = 1;
    g_menu_overlay_state = 2;
    thread_suspend(g_thread_current_id + 1);
    thread_suspend(g_thread_current_id + 2);
    thread_suspend(g_thread_current_id + 3);
    thread_suspend(g_thread_current_id + 4);
    for (;;) {
        g_menu_transition_state = g_at_list_state.selected_index + 100;
        if (g_at_list_selection_finished != 0) {
            break;
        }
        thread_yield();
    }
    g_at_list_selection_finished = 0;
    g_menu_transition_state = 0;
    unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);
    g_text_section_pointers[TEXT_SECTION_AT_LIST] = g_at_list_text;
    thread_resume(g_thread_current_id + 1);
    thread_resume(g_thread_current_id + 2);
    thread_resume(g_thread_current_id + 3);
    thread_resume(g_thread_current_id + 4);
    g_menu_modal_open = 0;
    g_menu_overlay_state = 0;
    thread_exit_current();
}

/* Runs the g_at_list_state menu on the thread two below the current one and
 * waits until that thread and the one below it have finished. */
void at_list_start_thread(void) {
    menu_entry_t* list = &g_at_list_state;
    menu_entry_t* saved = g_menu_thread_menu_data;

    g_at_list_state.select_text_table = 0x13;
    g_menu_thread_menu_data = list;
    g_at_list_state.context = &g_at_list_entry_table;
    at_list_init(list);
    thread_start(g_thread_current_id - 2, at_list_run);
    thread_set_parameters(g_thread_current_id - 2, (s32)list, 0, 0);
    do {
        thread_yield();
    } while (
        thread_is_running_asm(g_thread_current_id - 2) != 0 || thread_is_running_asm(g_thread_current_id - 3) != 0);
    g_menu_thread_menu_data = saved;
    thread_exit_current();
}
