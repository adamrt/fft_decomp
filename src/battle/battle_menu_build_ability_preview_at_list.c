/*
 * WORLD twin: world_menu_build_ability_preview_at_list
 * (same code, globals and callees rebound).
 */
#include "fft/battle.h"
#include "psx/types.h"

/*
 * One AT-list (turn order) descriptor, the BATTLE copy of the record
 * option_menu_init_at_list walks as option_at_descriptor_t. The four-byte
 * stride is proven by the index register the target advances by 4 and by the
 * +1 byte read for the turn value.
 *
 * flags bits: 0x1f = battle unit id, 0x1f meaning "end of list";
 *             0x20 = fixed "wait" label instead of a turn value;
 *             0x40 = the entry is the acting/queued unit (bracketed name);
 *             0x80 = turn value biased by 0x100.
 */
typedef struct battle_menu_at_descriptor {
    u8 flags;
    u8 turn_value;
    u8 _unused_02;
    u8 _unused_03;
} battle_menu_at_descriptor_t;

/*
 * Provisional: the AT-list work area this builder fills, reached through
 * g_event_overlay_load_address. Every offset below is written or published by
 * this function: the five parallel halfword columns become the layout's
 * text_ids/text_colors tables at the end, `text` is published through
 * g_menu_text_pointer, and `request` is the block handed to the main-stack
 * preview call, whose descriptor array follows it.
 */
typedef struct battle_menu_at_list_work {
    s16 primary_values[40];                      /* 0x000: turn-value column */
    s16 secondary_values[40];                    /* 0x050: unit-name column */
    s16 entry_numbers[40];                       /* 0x0a0: ordinal column */
    s16 primary_colors[40];                      /* 0x0f0 */
    s16 secondary_colors[40];                    /* 0x140 */
    u8 text[0x4f0];                              /* 0x190: packed 0xfe-terminated names */
    s32 request;                                 /* 0x680: preview request header */
    battle_menu_at_descriptor_t descriptors[40]; /* 0x684 */
} battle_menu_at_list_work_t;

typedef char battle_menu_at_list_work_descriptors_offset_must_be_0x684
    [((unsigned long)&((battle_menu_at_list_work_t*)0)->descriptors == 0x684) ? 1 : -1];

#define LAYOUT(entry) ((world_menu_scroll_text_layout_t*)(entry)->text_binding)

/*
 * Build the AT (turn order) list previewing the ability the player is about to
 * confirm: stage the selected ability or item into the shared action record at
 * g_battle_menu_selected_action, run battle_action_preview_at_list for the selected unit on the
 * main stack to obtain the previewed turn position and the descriptor list,
 * then write the name text and the four halfword columns and size menu entry
 * 52 around them.
 *
 * BATTLE twin of world_menu_build_ability_preview_at_list (WORLD), itself the twin of
 * option_menu_init_at_list from the loop onwards. The constructs below are
 * the WORLD twin's; see
 * src/world/world_menu_build_ability_preview_at_list.c for the evidence behind each one.
 *
 *   - `action` is a pointer local for the shared action record, so the store
 *     lands in the jal delay slot and the argument is `move a1,s0`.
 *   - The turn-value store is written ONCE after the if/else, and `0xb012`
 *     carries no (s16) cast (`ori v0,zero,0xb012`, not `addiu v1,zero,-20462`).
 *   - `text = published` is a PLAIN assignment written as the last statement
 *     before the `if`, so reorg's backward scan pulls it into the `beqz s7`
 *     delay slot.
 */
void battle_menu_build_ability_preview_at_list(void) {
    battle_menu_at_list_work_t* work;
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
    work = (battle_menu_at_list_work_t*)g_event_overlay_load_address;
    g_battle_menu_thread_menu_data[52].select_text_table = 0x13;
    g_battle_menu_thread_menu_data[52].text_binding = (struct world_menu_text_binding*)&g_battle_menu_at_list_layout;
    text_id = at_table[g_battle_menu_thread_menu_data[3].selected_index];
    g_battle_menu_selected_action.targeting_type = 5;
    entry = &g_battle_menu_thread_menu_data[52];
    if ((text_id & 0xf800) == TEXT_ID_ABILITY_NAME_BASE) {
        staged_item = 0;
        g_battle_menu_selected_action.ability_id = text_id - TEXT_ID_ABILITY_NAME_BASE;
    } else {
        staged_item = 1;
        g_battle_menu_selected_action.item_id = (u8)text_id;
    }
    action = &g_battle_menu_selected_action;
    action->unit_id = (u8)g_battle_active_turn_unit.battle_id;
    stats = battle_unit_get_attacker_data_pointer();
    g_battle_thread_call_target = (void (*)(void))battle_action_preview_at_list;
    turn = battle_thread_call_on_main_stack(stats, action, &work->request);
    if (staged_item != 0) {
        work->descriptors[0].flags |= 0x40;
        work->descriptors[0].turn_value = g_battle_menu_selected_action.item_id;
    }
    count = 0;
    if (turn < 0 || turn == 0xff) {
        g_battle_menu_thread_menu_data[53].text_id = 0x1023;
        battle_thread_set_parameters(g_battle_current_thread_id, (s32)&g_battle_menu_thread_menu_data[53], 0, 0);
        count = 0;
        battle_menu_icon_linked_entry_thread();
    }
    for (index = 0; index < 40; index++) {
        work->primary_colors[index] = 0;
        work->secondary_colors[index] = 0;
    }
    published = work->text;
    g_menu_text_pointer = published;
    count = 0;
    bracketed = 0;
    /* The target publishes the buffer from a temporary and copies it into the
     * callee-saved register the loop advances. The copy is a plain assignment
     * written as the LAST statement before the `if`: reorg scans back from the
     * branch and pulls it into the delay slot, which is where the target has
     * it. */
    text = published;
    if (turn != 0) {
        turn--;
    }
    for (index = 0; index < 39; index++) {
        battle_menu_at_descriptor_t* slot = &work->descriptors[index];
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
            s32 turn_text_id;
            s32 turn_bias;

            bracketed = 1;
            if ((slot->flags & 0x20) != 0) {
                turn_text_id = 0xb012;
            } else {
                turn_bias = slot->turn_value;
                if (index == 0 && staged_item != 0) {
                    turn_text_id = turn_bias + 0x3800;
                } else {
                    if ((slot->flags & 0x80) != 0) {
                        turn_bias += 0x100;
                    }
                    turn_text_id = turn_bias + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            work->primary_values[count] = turn_text_id;
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
    g_battle_menu_at_list_layout.columns.mode[0] = 3;
    g_battle_menu_at_list_layout.columns.mode[1] = 0;
    g_battle_menu_at_list_layout.columns.mode[2] = 0;
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
        g_battle_menu_at_list_layout.columns.mode[2] = 2;
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
    g_battle_menu_at_list_layout.columns.text_ids[0] = work->entry_numbers;
    g_battle_menu_at_list_layout.columns.text_ids[2] = work->secondary_values;
    g_battle_menu_at_list_layout.columns.text_colors[0] = work->primary_colors;
    g_battle_menu_at_list_layout.columns.text_ids[1] = work->primary_values;
    g_battle_menu_at_list_layout.columns.text_colors[1] = work->secondary_colors;
    g_battle_menu_at_list_layout.columns.text_colors[2] = work->secondary_colors;
}
