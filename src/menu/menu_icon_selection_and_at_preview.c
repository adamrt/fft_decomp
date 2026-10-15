/* Icon primitive submission, icon selection loop, auto-battle menu and ability AT list preview. */

#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"

void menu_submit_icon_primitives(menu_icon_sprites_t* frame) {
    gfx_draw_or_append_gpu_primitive(&frame->sprites[3]);
    gfx_draw_or_append_gpu_primitive(&frame->sprites[1]);
    gfx_draw_or_append_gpu_primitive(&frame->sprites[2]);
    gfx_draw_or_append_gpu_primitive(&frame->draw_mode);
    gfx_draw_or_append_gpu_primitive(&frame->sprites[0]);
    gfx_draw_or_append_gpu_primitive(&frame->first_draw_mode);
}

/* Run the five-policy auto-battle menu and update its action rows. */
void menu_auto_battle_setting_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    s32 previous;
    unit_stats_t* stats;
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 j;
    s32 fresh;
    s32 before;
    s32 after;
    s32 selected;
    s16* color;
    menu_icon_record_t* record;

    param = (menu_entry_t*)thread_get_current_parameter_1_variant();
    cursor = param->selected_index;
    stats = menu_get_selected_unit_stats();
    if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
        cursor = 0;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_TARGETED) {
        cursor = 1;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_RECOVERY) {
        cursor = 2;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_FADING_LIFE) {
        cursor = 3;
    } else {
        cursor = 4;
    }
    previous = cursor;
    menu_build_window_sprites_variant(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(records[1]));
    fresh = 0;
    i = 0;
    color = g_menu_auto_battle_option_colors;
    for (;; i++) {
        record = &records[i & 1];
        if (i == 0 || (g_script_event_input & PSX_PAD_CIRCLE)) {
            buffer = menu_build_and_upload_window_frame_image_variant(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            if (((g_script_event_input & PSX_PAD_CIRCLE) && thread_is_previous_running_variant() == 0) || i == 0) {
                selected = cursor;
                if (selected == 0) {
                    before = battle_menu_get_id_based_on_mount_moveable_actable(g_menu_selected_unit.battle_id);
                    stats->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                    g_menu_pending_selection[0] = 0x12;
                    color[0] = 8;
                    color[1] = 4;
                    color[2] = 4;
                    color[3] = 4;
                    color[4] = 4;
                    after = battle_menu_get_id_based_on_mount_moveable_actable(g_menu_selected_unit.battle_id);
                    if (before != after) {
                        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
                        menu_build_idle_action_menu_variant(after);
                    }
                } else {
                    color[0] = 4;
                    color[1] = 0;
                    color[2] = 0;
                    color[3] = 0;
                    color[4] = 0;
                    color[selected] = 8;
                }
                g_menu_auto_battle_row_actions[1] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[2] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[3] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[4] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                if (cursor == previous) {
                    g_menu_auto_battle_row_actions[1] = BATTLE_MENU_ID_AUTO_BATTLE_TARGET;
                    g_menu_auto_battle_row_actions[2] = BATTLE_MENU_ID_AUTO_BATTLE_TARGET;
                }
                previous = cursor;
                g_menu_thread_menu_data[BATTLE_MENU_ID_AUTO_BATTLE_TARGET].selected_index = 0;
                menu_handle_action(param, previous);
            }
            for (j = 0; j < 5; j++) {
                g_menu_text_palette_offset = color[j];
                g_menu_text_state.pen.stride = param->inner_width;
                menu_set_text_origin(8, j * 16 + 9);
                menu_display_text_entry(j + 0x5009, buffer, &g_menu_text_state.pen);
            }
            LoadImage(&rect, (u32*)buffer);
            fresh = 1;
        }
        thread_yield();
        if (fresh != 0) {
            menu_free_memory(buffer);
            fresh = 0;
        }
        param->selected_index = cursor;
        if (menu_should_close_thread_variant(&g_script_event_input) != 0) {
            break;
        }
        menu_step_wrapping_cursor_on_scroll_buttons(param, &cursor);
        menu_handle_cancel_input(param);
        menu_select_icon_cluts(&record->base);
        menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    thread_exit_current();
}

/* Run a wrapping icon menu; parameter 2 controls whether it exits its thread. */
void menu_run_icon_selection_loop(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    menu_icon_record_t* record;

    param = (menu_entry_t*)thread_get_current_parameter_1_variant();
    record = &records[0];
    cursor = param->selected_index;
    if (cursor == -1) {
        cursor = 0;
    }
    menu_build_window_sprites_variant(&rect, param, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    buffer = menu_build_and_upload_window_frame_image_variant(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, (u32*)buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        record = &records[i & 1];
        param->selected_index = cursor;
        menu_select_icon_cluts(&record->base);
        menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        if ((records[0]._unknown_78 == 0 && records[1]._unknown_78 == 2)
            || (records[0]._unknown_78 == 2 && records[1]._unknown_78 == 0)) {
            records[0]._unknown_78 = 0;
            records[1]._unknown_78 = 0;
        }
        if (menu_should_close_thread_variant(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(param, cursor);
        menu_step_wrapping_cursor_on_scroll_buttons(param, &cursor);
        menu_handle_cancel_input(param);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    if (thread_get_current_parameter_2_variant() == 0) {
        thread_exit_current();
    }
}

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

/* Thread body started from the ability list: build the AT list preview for the
 * highlighted ability, pulse tutorial wait value 0xfa and run it as a scrolling
 * list. */
void menu_preview_ability_turn_at_list(void) {
    menu_build_ability_preview_at_list();
    script_pulse_tutorial_wait_value(0xFA);
    menu_run_scrolling_ability_list_thread_variant();
}
