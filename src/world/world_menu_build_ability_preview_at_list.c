#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/menu_types.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Provisional: the layout record at 0x80153cd0 bound into menu entry 52. Its
 * head is exactly world_menu_text_layout_t; the halfword at +0x28 is the
 * scroll row this function sets and world_menu_draw_text_columns receives as
 * its `row_offset` argument. Proposed as a `s16 scroll_row;` field appended to
 * world_menu_text_layout_t in include/fft/world.h (size assert 0x28 -> 0x2c).
 */
#define LAYOUT(entry) ((world_menu_scroll_text_layout_t*)(entry)->text_binding)

/*
 * Build the AT (turn order) list previewing the ability the player is about to
 * confirm: stage the selected ability or item into the shared action record at
 * g_world_menu_preview_action, run battle_action_preview_at_list for the stored unit on the
 * main stack to obtain the previewed turn position and the descriptor list,
 * then write the name text and the four halfword columns and size menu entry
 * 52 around them.
 *
 * WORLD twin of option_menu_init_at_list from the loop onwards. As in that
 * twin the window geometry is written per branch, because the store through
 * the layout pointer may alias the entry record, and two counters that always
 * hold the same value are kept apart: the loop index addresses the descriptor
 * table and the ordinal column, the accepted-entry counter the value and flag
 * columns. Merging them makes every column share one induction variable and
 * loses the ordinal column's recomputed shift.
 *
 * Load-bearing constructs:
 *
 *   - The 0x80 flag test reads `slot->flags` through a volatile lvalue: the
 *     target reloads that byte after the turn value instead of reusing the
 *     earlier load.
 *   - `action` is a pointer local for the shared action record. The target
 *     keeps that address in s0, stores through it in the jal delay slot and
 *     passes `move a1,s0`; storing to g_world_menu_preview_action absolutely wastes the delay
 *     slot on a nop and rematerialises lui a1/addiu a1.
 *   - The turn-value store is written once after the if/else. The target's
 *     0xb012 arm is `j 0xed3f8` with `li v0,0xb012` in the delay slot, landing
 *     on a store shared with the computed arm.
 *   - `0xb012` carries no (s16) cast: the cast emits `addiu v1,zero,-20462`
 *     where the target has `ori v0,zero,0xb012`. Both store identical bits.
 *   - `text = published` is a plain assignment, written as the last statement
 *     before the `if` (see the comment there). Both names are function-scope,
 *     so the copy survives without any construct, and reorg's backward scan
 *     pulls it into the delay slot -- the target's `beqz s7` / `move s0,v0`.
 *     Written before `count`/`bracketed`, the copy is stranded ahead of the
 *     branch. Binding `text` to $16 instead coalesces the producer
 *     (`addiu s0,s5,400`) into the hard register and deletes the copy.
 */
void world_menu_build_ability_preview_at_list(void) {
    world_menu_at_list_work_t* work;
    world_menu_entry_t* entry;
    battle_stats_t* stats;
    u8* text;
    battle_ai_command_action_t* action;
    u8* published;
    s32 turn;
    s32 count;
    s32 index;
    s32 bracketed;
    s32 staged_item;
    u16 text_id;
    const u16* at_table;

    at_table = (const u16*)g_battle_ai_workspace_ptr;
    work = (world_menu_at_list_work_t*)g_event_overlay_load_address;
    g_world_menu_thread_menu_data[52].select_text_table = 0x13;
    g_world_menu_thread_menu_data[52].text_binding = (struct world_menu_text_binding*)&g_world_menu_at_list_layout;
    text_id = at_table[g_world_menu_thread_menu_data[3].selected_index];
    g_world_menu_preview_action.targeting_type = 5;
    entry = &g_world_menu_thread_menu_data[52];
    if ((text_id & 0xf800) == TEXT_ID_ABILITY_NAME_BASE) {
        staged_item = 0;
        g_world_menu_preview_action.ability_id = text_id - TEXT_ID_ABILITY_NAME_BASE;
    } else {
        staged_item = 1;
        g_world_menu_preview_action.item_id = (u8)text_id;
    }
    action = &g_world_menu_preview_action;
    action->unit_id = (u8)g_world_unit_view_battle_id;
    stats = world_unit_get_battle_stats_for_stored();
    g_world_thread_call_target = (void (*)(void))battle_action_preview_at_list;
    turn = world_thread_call_on_main_stack(stats, action, &work->request);
    if (staged_item != 0) {
        work->descriptors[0].flags |= 0x40;
        work->descriptors[0].turn_value = g_world_menu_preview_action.item_id;
    }
    count = 0;
    if (turn < 0 || turn == 0xff) {
        g_world_menu_thread_menu_data[53].text_id = 0x1023;
        world_thread_set_parameters(g_world_thread_current_id, (s32)&g_world_menu_thread_menu_data[53], 0, 0);
        count = 0;
        world_menu_icon_linked_entry_thread();
    }
    for (index = 0; index < 40; index++) {
        work->primary_colors[index] = 0;
        work->secondary_colors[index] = 0;
    }
    published = work->text;
    g_world_text_section_pointers[31] = published;
    count = 0;
    bracketed = 0;
    /* The target publishes the buffer from a temporary and copies it into the
     * callee-saved register the loop advances. The copy is a plain assignment
     * written as the LAST statement before the `if`: reorg scans back from the
     * branch and pulls it into the delay slot, which is where the target has
     * it. A two-variable tie also creates the copy, but its #APP emits
     * `.set reorder` and blocks that backward scan, stranding the copy ahead
     * of the branch and spending the slot on `bracketed = 0` instead. */
    text = published;
    if (turn != 0) {
        turn--;
    }
    for (index = 0; index < 39; index++) {
        world_at_descriptor_t* slot = &work->descriptors[index];
        s32 name_index;

        if ((slot->flags & 0x1f) == 0x1f) {
            break;
        }
        work->entry_numbers[index] = index + 1;
        stats = battle_unit_get_stats_from_battle_id(slot->flags & 0x1f);
        if ((slot->flags & 0x40) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbe;
        }
        for (name_index = 0; name_index < 16; name_index++) {
            u8 character = stats->name[name_index];
            *text = character;
            if ((character & 0xfe) == 0xfe) {
                break;
            }
            text++;
        }
        if ((slot->flags & 0x40) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbf;
        }
        *text++ = 0xfe;
        if ((slot->flags & 0x40) != 0) {
            work->secondary_values[count] = index - 0x800;
        } else {
            work->primary_values[count] = index - 0x800;
        }
        if ((slot->flags & 0x40) == 0) {
            work->secondary_values[count] = TEXT_ID_ABILITY_NAME_BASE;
        } else {
            s32 value;
            s32 turn_bias;

            bracketed = 1;
            if ((slot->flags & 0x20) != 0) {
                value = 0xb012;
            } else {
                turn_bias = slot->turn_value;
                if (index != 0 || staged_item == 0) {
                    if ((*(volatile u8*)&slot->flags & 0x80) != 0) {
                        turn_bias += 0x100;
                    }
                    value = turn_bias + TEXT_ID_ABILITY_NAME_BASE;
                } else {
                    value = turn_bias + 0x3800;
                }
            }
            work->primary_values[count] = value;
        }
        if ((stats->initial_team_flags & BATTLE_TEAM_MASK) != 0) {
            work->secondary_colors[count] = 8;
        }
        count++;
    }
    if (count >= 39) {
        count = 39;
    }
    work->primary_colors[turn] = 4;
    work->secondary_colors[turn] = 4;
    g_world_menu_at_list_layout.columns.mode[0] = 3;
    g_world_menu_at_list_layout.columns.mode[1] = 0;
    g_world_menu_at_list_layout.columns.mode[2] = 0;
    if (bracketed != 0) {
        LAYOUT(entry)->columns.row_count = 6;
        entry->window_x = 0x8c;
        entry->window_y = 0x30;
        entry->window_width = 0xe8;
        entry->overall_width = 0xe8;
        entry->inner_width = 0xe8;
    } else {
        LAYOUT(entry)->columns.row_count = 6;
        entry->window_x = 0xbe;
        entry->window_y = 0x30;
        entry->window_width = 0x84;
        entry->overall_width = 0x84;
        entry->inner_width = 0x84;
        g_world_menu_at_list_layout.columns.mode[2] = 2;
    }
    if (count < 6) {
        entry->window_y = (6 - count) * 8 + 0x20;
        LAYOUT(entry)->columns.row_count = count;
        LAYOUT(entry)->columns.hidden_rows = 0;
    } else {
        LAYOUT(entry)->columns.hidden_rows = count - 6;
    }
    entry->selected_index = turn;
    if (turn < 3) {
        LAYOUT(entry)->row_offset = 0;
    } else if (turn >= count - 6) {
        LAYOUT(entry)->row_offset = count - 6;
    } else {
        LAYOUT(entry)->row_offset = turn - 3;
    }
    g_world_menu_at_list_layout.columns.text_ids[0] = work->entry_numbers;
    g_world_menu_at_list_layout.columns.text_ids[2] = work->secondary_values;
    g_world_menu_at_list_layout.columns.text_colors[0] = work->primary_colors;
    g_world_menu_at_list_layout.columns.text_ids[1] = work->primary_values;
    g_world_menu_at_list_layout.columns.text_colors[1] = work->secondary_colors;
    g_world_menu_at_list_layout.columns.text_colors[2] = work->secondary_colors;
}
