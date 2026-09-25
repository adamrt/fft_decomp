#include "fft/battle.h"
#include "fft/option.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * One AT-list (turn order) descriptor. The overlay keeps 0x28 of them at
 * 0x801cb6fc; the four-byte stride is proven by the strength-reduced index
 * register the target advances by 4 and by the +1 byte read for the turn
 * value.
 */
typedef struct option_at_descriptor {
    u8 flags;
    u8 turn_value;
    u8 unknown_02;
    u8 unknown_03;
} option_at_descriptor_t;

enum {
    OPTION_AT_UNIT_ID_MASK = 0x1f,
    OPTION_AT_END_OF_LIST = 0x1f,
    OPTION_AT_SHOW_WAIT_LABEL = 0x20,
    OPTION_AT_ACTING_UNIT = 0x40,
    OPTION_AT_TURN_VALUE_HIGH_BIT = 0x80,
};

/* option_scroll_layout_t and option_at_menu_t live in fft/option.h. */

extern option_at_descriptor_t g_option_menu_at_list_descriptors[];

/*
 * Rebuild the AT (turn order) list from the descriptor table and size the
 * menu window around it.
 *
 * The target keeps two counters that always hold the same value: the loop
 * index addresses the descriptor table and the entry-number column, while
 * the separate accepted-entry counter addresses the value and flag columns.
 * Merging them makes the four halfword columns share one induction variable
 * and loses the entry-number column's recomputed shift.
 *
 * The menu geometry is written per branch rather than through a shared
 * temporary: the store through the layout pointer may alias the menu record,
 * so the repeated window_y store is not redundant to the compiler.
 */
void option_menu_init_at_list(option_at_menu_t* menu) {
    s32 entry_count;
    s32 has_special_entry;
    s32 index;
    u8* text;

    g_battle_thread_call_target = (void (*)(void))battle_action_calculate_at_list;
    battle_thread_call_on_main_stack((u8*)g_option_menu_at_list_descriptors, 0);

    for (index = 0; index < 0x100; index++) {
        g_dead_unit_menu_flags[index] = 0;
        g_option_menu_at_list_flags[index] = 0;
    }

    g_menu_text_pointer = g_option_menu_at_list_text;
    entry_count = 0;
    has_special_entry = 0;
    text = g_option_menu_at_list_text;

    for (index = 0; index < 0x28; index++) {
        battle_stats_t* unit;
        s32 name_index;

        if ((g_option_menu_at_list_descriptors[index].flags & OPTION_AT_UNIT_ID_MASK) == OPTION_AT_END_OF_LIST) {
            break;
        }

        g_option_menu_at_list_indices[index] = index + 1;
        unit = battle_unit_get_stats_from_battle_id(
            g_option_menu_at_list_descriptors[index].flags & OPTION_AT_UNIT_ID_MASK);
        if ((g_option_menu_at_list_descriptors[index].flags & OPTION_AT_ACTING_UNIT) != 0) {
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
        if ((g_option_menu_at_list_descriptors[index].flags & OPTION_AT_ACTING_UNIT) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbf;
        }
        *text++ = 0xfe;

        if ((g_option_menu_at_list_descriptors[index].flags & OPTION_AT_ACTING_UNIT) != 0) {
            g_option_menu_at_list_secondary_values[entry_count] = index - 0x800;
        } else {
            g_option_menu_at_list_primary_values[entry_count] = index - 0x800;
        }

        if ((g_option_menu_at_list_descriptors[index].flags & OPTION_AT_ACTING_UNIT) == 0) {
            g_option_menu_at_list_secondary_values[entry_count] = TEXT_ID_ABILITY_NAME_BASE;
        } else {
            has_special_entry = 1;
            if ((g_option_menu_at_list_descriptors[index].flags & OPTION_AT_SHOW_WAIT_LABEL) != 0) {
                g_option_menu_at_list_primary_values[entry_count] = (s16)0xb012;
            } else {
                s32 value = g_option_menu_at_list_descriptors[index].turn_value;
                if ((g_option_menu_at_list_descriptors[index].flags & OPTION_AT_TURN_VALUE_HIGH_BIT) != 0) {
                    value += 0x100;
                }
                g_option_menu_at_list_primary_values[entry_count] = value + TEXT_ID_ABILITY_NAME_BASE;
            }
        }

        if ((unit->initial_team_flags & 0x30) != 0) {
            g_option_menu_at_list_flags[entry_count] = 8;
        }
        entry_count++;
    }

    if (entry_count >= 0x28) {
        entry_count = 0x27;
    }

    if (has_special_entry != 0) {
        menu->window_y = 0x20;
        menu->layout->step = 10;
        menu->window_x = 0x8c;
        menu->window_y = 0x20;
        menu->window_width = 0xe8;
        menu->overall_width = 0xe8;
        menu->inner_width = 0xe8;
    } else {
        menu->window_y = 0x20;
        menu->layout->step = 10;
        menu->window_x = 0xbe;
        menu->window_y = 0x20;
        menu->window_width = 0x84;
        menu->overall_width = 0x84;
        menu->inner_width = 0x84;
    }

    if (entry_count < 10) {
        menu->window_y = (10 - entry_count) * 8 + 0x20;
        menu->layout->step = entry_count;
        menu->layout->maximum = 0;
    } else {
        menu->layout->maximum = entry_count - 10;
    }
}
