#include "fft/battle_action.h"
#include "fft/event_option.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

/*
 * Rebuild the AT (turn order) list from the descriptor table and size the
 * menu window around it.
 *
 * The target keeps two counters that always hold the same value: the loop
 * index addresses the descriptor table and the entry-number column, while
 * the separate accepted-entry counter addresses the value and flag columns.
 * Merging them makes the four 16-bit columns share one induction variable
 * and loses the entry-number column's recomputed shift.
 *
 * The menu geometry is written per branch rather than through a shared
 * temporary: the store through the layout pointer may alias the menu record,
 * so the repeated window.y store is not redundant to the compiler.
 */
void menu_init_at_list(menu_entry_t* menu) {
    s32 entry_count;
    s32 has_special_entry;
    s32 index;
    u8* text;

    g_thread_call_target = (void (*)(void))action_calculate_at_list;
    thread_call_on_main_stack(g_option_menu_at_list_descriptors, 0);

    for (index = 0; index < 0x100; index++) {
        g_dead_unit_menu_flags[index] = 0;
        g_menu_at_list_flags[index] = 0;
    }

    g_text_section_pointers[TEXT_SECTION_AT_LIST] = g_menu_at_list_text;
    entry_count = 0;
    has_special_entry = 0;
    text = g_menu_at_list_text;

    for (index = 0; index < BATTLE_AT_LIST_CAPACITY; index++) {
        unit_stats_t* unit;
        s32 name_index;

        if ((g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_UNIT_ID_MASK) == BATTLE_AT_END_OF_LIST) {
            break;
        }

        g_menu_at_list_indices[index] = index + 1;
        unit = battle_unit_get_stats_from_battle_id(
            g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_UNIT_ID_MASK);
        if ((g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_ACTION_ENTRY) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbe;
        }
        /* The pointer must lead the index in the sum to match the target's
           operand order on the address add. */
        for (name_index = 0; name_index < 0x10; name_index++) {
            u8 character = unit->name[name_index];
            *text = character;
            if ((character & 0xfe) == 0xfe) {
                break;
            }
            text++;
        }
        if ((g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_ACTION_ENTRY) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbf;
        }
        *text++ = 0xfe;

        if ((g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_ACTION_ENTRY) != 0) {
            g_menu_at_list_secondary_values[entry_count] = index - 0x800;
        } else {
            g_menu_at_list_primary_values[entry_count] = index - 0x800;
        }

        if ((g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_ACTION_ENTRY) == 0) {
            g_menu_at_list_secondary_values[entry_count] = TEXT_ID_ABILITY_NAME_BASE;
        } else {
            has_special_entry = 1;
            if ((g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_JUMP) != 0) {
                g_menu_at_list_primary_values[entry_count] = 0xb012;
            } else {
                s32 value = g_option_menu_at_list_descriptors[index].action_id_low;
                if ((g_option_menu_at_list_descriptors[index].unit_code & BATTLE_AT_ABILITY_ID_HIGH_BIT) != 0) {
                    value += 0x100;
                }
                g_menu_at_list_primary_values[entry_count] = value + TEXT_ID_ABILITY_NAME_BASE;
            }
        }

        if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE) {
            g_menu_at_list_flags[entry_count] = 8;
        }
        entry_count++;
    }

    if (entry_count >= BATTLE_AT_LIST_CAPACITY) {
        entry_count = BATTLE_AT_LIST_CAPACITY - 1;
    }

    if (has_special_entry != 0) {
        menu_text_layout_t* layout;

        menu->window.y = 0x20;
        layout = menu->context;
        layout->row_count = 10;
        menu->window.x = 0x8c;
        menu->window.y = 0x20;
        menu->window.w = 0xe8;
        menu->text_image.width = 0xe8;
        menu->inner_width = 0xe8;
    } else {
        menu_text_layout_t* layout;

        menu->window.y = 0x20;
        layout = menu->context;
        layout->row_count = 10;
        menu->window.x = 0xbe;
        menu->window.y = 0x20;
        menu->window.w = 0x84;
        menu->text_image.width = 0x84;
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
