#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Build the selected ability/item AT preview and its three-column menu. */
void menu_build_ability_preview_at_list(void) {
    menu_at_list_work_t* work;
    menu_entry_t* entry;
    unit_stats_t* stats;
    u8* text;
    action_command_t* action;
    u8* published;
    s32 turn;
    s32 count;
    s32 index;
    s32 bracketed;
    s32 staged_item;
    u16 text_id;
    const u16* at_table;

    at_table = g_ai_workspace_ptr->ability_list.ids;
    work = (menu_at_list_work_t*)g_main_heap_companion_overlay_load_address;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_PREVIEW_AT_LIST].select_text_table = 0x13;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_PREVIEW_AT_LIST].context = &g_menu_at_list_layout;
    text_id = at_table[g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index];
    g_menu_selected_action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    entry = &g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_PREVIEW_AT_LIST];
    if ((text_id & 0xf800) == TEXT_ID_ABILITY_NAME_BASE) {
        staged_item = 0;
        g_menu_selected_action.ability_id = text_id - TEXT_ID_ABILITY_NAME_BASE;
    } else {
        staged_item = 1;
        /* Item selection updates only the low byte of the 16-bit command field. */
        *(u8*)&g_menu_selected_action.item_or_equipment = text_id;
    }
    action = &g_menu_selected_action;
    action->actor_id = g_menu_selected_unit.battle_id;
    stats = menu_get_selected_unit_stats();
    g_thread_call_target = (void (*)(void))action_preview_at_list;
    turn = thread_call_on_main_stack(stats, action, work->entries);
    if (staged_item != 0) {
        work->entries[1].unit_code |= BATTLE_AT_ACTION_ENTRY;
        work->entries[1].action_id_low = g_menu_selected_action.item_or_equipment;
    }
    count = 0;
    if (turn < 0 || turn == 0xff) {
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_PREVIEW_MESSAGE].text_id = 0x1023;
        thread_set_parameters(
            g_thread_current_id, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_PREVIEW_MESSAGE], 0, 0);
        count = 0;
        menu_icon_linked_entry_thread();
    }
    for (index = 0; index < BATTLE_AT_LIST_CAPACITY; index++) {
        work->primary_colors[index] = 0;
        work->secondary_colors[index] = 0;
    }
    published = work->text;
    g_text_section_pointers[TEXT_SECTION_AT_LIST] = published;
    count = 0;
    bracketed = 0;
    /* Keep the published text cursor separate before the turn adjustment. */
    text = published;
    if (turn != 0) {
        turn--;
    }
    for (index = 0; index < BATTLE_AT_LIST_CAPACITY - 1; index++) {
        battle_at_entry_t* slot = &work->entries[index + 1];
        s32 name_index;

        if ((slot->unit_code & BATTLE_AT_UNIT_ID_MASK) == BATTLE_AT_END_OF_LIST) {
            break;
        }
        work->entry_numbers[index] = index + 1;
        stats = battle_unit_get_stats_from_battle_id(slot->unit_code & BATTLE_AT_UNIT_ID_MASK);
        if ((slot->unit_code & BATTLE_AT_ACTION_ENTRY) != 0) {
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
        if ((slot->unit_code & BATTLE_AT_ACTION_ENTRY) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbf;
        }
        *text++ = 0xfe;
        if ((slot->unit_code & BATTLE_AT_ACTION_ENTRY) != 0) {
            work->secondary_values[count] = index - 0x800;
        } else {
            work->primary_values[count] = index - 0x800;
        }
        if ((slot->unit_code & BATTLE_AT_ACTION_ENTRY) == 0) {
            work->secondary_values[count] = TEXT_ID_ABILITY_NAME_BASE;
        } else {
            s32 turn_text_id;
            s32 turn_bias;

            bracketed = 1;
            if ((slot->unit_code & BATTLE_AT_JUMP) != 0) {
                turn_text_id = 0xb012;
            } else {
                turn_bias = slot->action_id_low;
                if (index == 0 && staged_item != 0) {
                    turn_text_id = turn_bias + 0x3800;
                } else {
                    if ((slot->unit_code & BATTLE_AT_ABILITY_ID_HIGH_BIT) != 0) {
                        turn_bias += 0x100;
                    }
                    turn_text_id = turn_bias + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            work->primary_values[count] = turn_text_id;
        }
        if ((stats->initial_team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE) {
            work->secondary_colors[count] = 8;
        }
        count++;
    }
    if (count >= BATTLE_AT_LIST_CAPACITY - 1) {
        count = BATTLE_AT_LIST_CAPACITY - 1;
    }
    work->primary_colors[turn] = 4;
    work->secondary_colors[turn] = 4;
    g_menu_at_list_layout.columns.mode[0] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_at_list_layout.columns.mode[1] = MENU_TEXT_COLUMN_TEXT;
    g_menu_at_list_layout.columns.mode[2] = MENU_TEXT_COLUMN_TEXT;
    if (bracketed != 0) {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->columns.row_count = 6;
        entry->window.x = 0x8c;
        entry->window.y = 0x30;
        entry->window.w = 0xe8;
        entry->text_image.width = 0xe8;
        entry->inner_width = 0xe8;
    } else {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->columns.row_count = 6;
        entry->window.x = 0xbe;
        entry->window.y = 0x30;
        entry->window.w = 0x84;
        entry->text_image.width = 0x84;
        entry->inner_width = 0x84;
        g_menu_at_list_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
    }
    if (count < 6) {
        menu_scroll_text_layout_t* layout;
        menu_scroll_text_layout_t* hidden_rows_layout;

        entry->window.y = (6 - count) * 8 + 0x20;
        layout = entry->context;
        layout->columns.row_count = count;
        hidden_rows_layout = entry->context;
        hidden_rows_layout->columns.hidden_rows = 0;
    } else {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->columns.hidden_rows = count - 6;
    }
    entry->selected_index = turn;
    if (turn < 3) {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->row_offset = 0;
    } else if (turn >= count - 6) {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->row_offset = count - 6;
    } else {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->row_offset = turn - 3;
    }
    g_menu_at_list_layout.columns.text_ids[0] = work->entry_numbers;
    g_menu_at_list_layout.columns.text_ids[2] = work->secondary_values;
    g_menu_at_list_layout.columns.text_colors[0] = work->primary_colors;
    g_menu_at_list_layout.columns.text_ids[1] = work->primary_values;
    g_menu_at_list_layout.columns.text_colors[1] = work->secondary_colors;
    g_menu_at_list_layout.columns.text_colors[2] = work->secondary_colors;
}
