/* Icon primitives, the auto battle setting, icon selection and ability preview AT lists. */

#include "fft/ability_list.h"
#include "fft/ai.h"
#include "fft/at_list.h"
#include "fft/command.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/target.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern menu_scroll_text_layout_t g_at_list_layout;
extern s16 g_auto_battle_option_colors[5]; /* text color per option */

/* Provisional: the AT-list work area at_list_build_ability_preview
 * fills, reached through g_heap_companion_overlay_load_address. The five
 * parallel 16-bit columns become the layout's text_ids/text_colors tables,
 * `text` is published through g_text_section_pointers[TEXT_SECTION_AT_LIST].
 * The main-stack preview fills all 40 AT entries; the menu displays entries
 * 1 through 39. */
typedef struct at_list_work {
    s16 primary_values[AT_LIST_CAPACITY];   /* 0x000: turn-value column */
    s16 secondary_values[AT_LIST_CAPACITY]; /* 0x050: unit-name column */
    s16 entry_numbers[AT_LIST_CAPACITY];    /* 0x0a0: ordinal column */
    s16 primary_colors[AT_LIST_CAPACITY];   /* 0x0f0 */
    s16 secondary_colors[AT_LIST_CAPACITY]; /* 0x140 */
    u8 text[0x4f0];                         /* 0x190: packed 0xfe-terminated names */
    at_entry_t entries[AT_LIST_CAPACITY];   /* 0x680; entry 0 is omitted from the menu */
} at_list_work_t;
typedef char at_list_work_size_must_be_1824[(sizeof(at_list_work_t) == 1824) ? 1 : -1];

/* Row actions of the auto-battle setting menu (menu entry 0x80166b4c, +0x24). */
static s16 g_auto_battle_row_actions[8] = { 1000, 2, 2, 2, 2, 0, -3, -3 };

/* Draw or queue an icon frame's six packets: sprites 3, 1 and 2, the draw mode, sprite 0, then the
 * first draw mode. Queued into one ordering-table entry they run in the reverse order. */
void menu_submit_icon_primitives(menu_icon_sprites_t* frame) {
    gfx_draw_or_append_gpu_primitive(&frame->sprites[3]);
    gfx_draw_or_append_gpu_primitive(&frame->sprites[1]);
    gfx_draw_or_append_gpu_primitive(&frame->sprites[2]);
    gfx_draw_or_append_gpu_primitive(&frame->draw_mode);
    gfx_draw_or_append_gpu_primitive(&frame->sprites[0]);
    gfx_draw_or_append_gpu_primitive(&frame->first_draw_mode);
}

/* The Auto-battle settings window: Manual and the four AI policies, with the cursor
 * starting on the selected unit's current setting.
 *
 * On the first frame and on each Circle it redraws the five option names (text 0x5009 on)
 * and acts on the cursor row. Manual takes effect at once, and rebuilds the idle action menu
 * when that changes which menu the unit gets. A policy row only becomes the highlighted one;
 * its row action leads to AUTO_BATTLE_CONFIRM, or for rows 1 and 2 chosen again to
 * AUTO_BATTLE_TARGET. Text palette 8 marks the active row. */
void auto_battle_settings_thread(void) {
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

    param = (menu_entry_t*)thread_get_current_parameter_1();
    cursor = param->selected_index;
    stats = menu_get_selected_unit_stats();
    if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
        cursor = 0;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_FIGHT_FOR_LIFE) {
        cursor = 1;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_PROTECT_ALLIES) {
        cursor = 2;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_FADING_LIFE) {
        cursor = 3;
    } else {
        cursor = 4;
    }
    previous = cursor;
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(records[1]));
    fresh = 0;
    i = 0;
    color = g_auto_battle_option_colors;
    for (;; i++) {
        record = &records[i & 1];
        if (i == 0 || (g_script_event_input & PSX_PAD_CIRCLE)) {
            buffer = menu_build_and_upload_window_frame_image(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            if (((g_script_event_input & PSX_PAD_CIRCLE) && thread_is_previous_running() == 0) || i == 0) {
                selected = cursor;
                if (selected == 0) {
                    before = menu_action_get_menu_id(g_unit_panel_selected_unit.battle_id);
                    stats->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                    g_command_pending_selection[0] = 0x12;
                    color[0] = 8;
                    color[1] = 4;
                    color[2] = 4;
                    color[3] = 4;
                    color[4] = 4;
                    after = menu_action_get_menu_id(g_unit_panel_selected_unit.battle_id);
                    if (before != after) {
                        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
                        command_open_menu(after);
                    }
                } else {
                    color[0] = 4;
                    color[1] = 0;
                    color[2] = 0;
                    color[3] = 0;
                    color[4] = 0;
                    color[selected] = 8;
                }
                g_auto_battle_row_actions[1] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                g_auto_battle_row_actions[2] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                g_auto_battle_row_actions[3] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                g_auto_battle_row_actions[4] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                if (cursor == previous) {
                    g_auto_battle_row_actions[1] = MENU_COMMAND_ID_AUTO_BATTLE_TARGET;
                    g_auto_battle_row_actions[2] = MENU_COMMAND_ID_AUTO_BATTLE_TARGET;
                }
                previous = cursor;
                g_menu_thread_menu_data[MENU_COMMAND_ID_AUTO_BATTLE_TARGET].selected_index = 0;
                menu_handle_action(param, previous);
            }
            for (j = 0; j < 5; j++) {
                g_menu_text_palette_offset = color[j];
                g_menu_window_text_pen.stride = param->inner_width;
                menu_set_text_origin(8, j * 16 + 9);
                menu_display_text_entry(j + 0x5009, buffer, &g_menu_window_text_pen);
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
        if (menu_should_close_thread(&g_script_event_input) != 0) {
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

    param = (menu_entry_t*)thread_get_current_parameter_1();
    record = &records[0];
    cursor = param->selected_index;
    if (cursor == -1) {
        cursor = 0;
    }
    menu_build_window_sprites(&rect, param, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    buffer = menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_window_text_pen.stride = param->inner_width;
    menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    menu_display_text_entry(param->text_id, buffer, &g_menu_window_text_pen);
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
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(param, cursor);
        menu_step_wrapping_cursor_on_scroll_buttons(param, &cursor);
        menu_handle_cancel_input(param);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}

/* Preview where the highlighted ability or item would fall in the turn order.
 *
 * at_list_preview runs on the main stack for the selected unit and fills the
 * AT list work area in the companion overlay space. The names of the listed units are written
 * into the AT list text section, and the ABILITY_PREVIEW_AT_LIST entry is laid out with
 * number, unit and action columns (the action column and wide window only when an action row
 * is listed), scrolled so the previewed row shows. When the action gets no turn, this
 * thread shows ABILITY_PREVIEW_MESSAGE (text 0x1023) instead and exits there. */
static void at_list_build_ability_preview(void) {
    at_list_work_t* work;
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
    const u16* at_table = g_ai_workspace_ptr->ability_list.ids;

    work = (at_list_work_t*)g_heap_companion_overlay_load_address;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_AT_LIST].select_text_table = 0x13;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_AT_LIST].context = &g_at_list_layout;
    text_id = at_table[g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index];
    g_command_selected_command.data.action.targeting_type = TARGET_TYPE_TILE;
    entry = &g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_AT_LIST];

    if ((text_id & 0xf800) == TEXT_ID_ABILITY_NAME_BASE) {
        staged_item = 0;
        g_command_selected_command.data.action.ability_id = text_id - TEXT_ID_ABILITY_NAME_BASE;
    } else {
        staged_item = 1;
        g_command_selected_command.data.action.item_id = text_id;
    }

    action = &g_command_selected_command.data.action;
    action->actor_id = g_unit_panel_selected_unit.battle_id;
    stats = menu_get_selected_unit_stats();
    g_thread_call_target = (void (*)(void))at_list_preview;
    turn = thread_call_on_main_stack(stats, action, work->entries);
    /* An item preview marks the first entry as the item action; the loop below names it
     * from the item text. */
    if (staged_item != 0) {
        work->entries[1].unit_code |= AT_ACTION_ENTRY;
        work->entries[1].action_id_low = g_command_selected_command.data.action.item_id;
    }

    /* Without an AT position for the action, this thread shows preview message 0x1023
     * and exits. */

    count = 0;
    if (turn < 0 || turn == 0xff) {
        g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_MESSAGE].text_id = 0x1023;
        thread_set_parameters(
            g_thread_current_id, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_MESSAGE], 0, 0);
        count = 0;
        menu_icon_linked_entry_thread();
    }

    for (index = 0; index < AT_LIST_CAPACITY; index++) {
        work->primary_colors[index] = 0;
        work->secondary_colors[index] = 0;
    }

    published = work->text;
    g_text_section_pointers[TEXT_SECTION_AT_LIST] = published;
    count = 0;
    bracketed = 0;
    /* Keep the published text cursor separate before the turn adjustment. */
    text = published;
    /* `turn` becomes the 0-based row of the previewed action. */
    if (turn != 0) {
        turn--;
    }

    for (index = 0; index < AT_LIST_CAPACITY - 1; index++) {
        at_entry_t* slot = &work->entries[index + 1];
        s32 name_index;

        if ((slot->unit_code & AT_UNIT_ID_MASK) == AT_END_OF_LIST) {
            break;
        }
        work->entry_numbers[index] = index + 1;
        stats = unit_get_stats(slot->unit_code & AT_UNIT_ID_MASK);
        /* Action rows wrap the unit name in the 0xd9be/0xd9bf glyph pair. */
        if ((slot->unit_code & AT_ACTION_ENTRY) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbe;
        }
        for (name_index = 0; name_index < UNIT_NAME_SIZE; name_index++) {
            u8 character = stats->name[name_index];
            *text = character;
            if ((character & 0xfe) == 0xfe) {
                break;
            }
            text++;
        }
        if ((slot->unit_code & AT_ACTION_ENTRY) != 0) {
            *text++ = 0xd9;
            *text++ = 0xbf;
        }
        *text++ = 0xfe;

        /* Text id 0xf800 + index is entry `index` of the AT list section, the name just
         * written: unit rows show it in the first text column, action rows in the second. */
        if ((slot->unit_code & AT_ACTION_ENTRY) != 0) {
            work->secondary_values[count] = index - 0x800;
        } else {
            work->primary_values[count] = index - 0x800;
        }
        if ((slot->unit_code & AT_ACTION_ENTRY) == 0) {
            work->secondary_values[count] = TEXT_ID_ABILITY_NAME_BASE;
        } else {
            s32 action_text_id;
            s32 action_id;

            /* The action's name: the jump text, the staged item's name or the ability's. */
            bracketed = 1;
            if ((slot->unit_code & AT_JUMP) != 0) {
                action_text_id = 0xb012;
            } else {
                action_id = slot->action_id_low;
                if (index == 0 && staged_item != 0) {
                    action_text_id = action_id + 0x3800;
                } else {
                    if ((slot->unit_code & AT_ABILITY_ID_HIGH_BIT) != 0) {
                        action_id += 0x100;
                    }
                    action_text_id = action_id + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            work->primary_values[count] = action_text_id;
        }

        if ((stats->initial_team_flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE) {
            work->secondary_colors[count] = 8;
        }
        count++;
    }

    if (count >= AT_LIST_CAPACITY - 1) {
        count = AT_LIST_CAPACITY - 1;
    }

    /* Color the previewed action's row. */
    work->primary_colors[turn] = 4;
    work->secondary_colors[turn] = 4;
    g_at_list_layout.columns.mode[0] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_at_list_layout.columns.mode[1] = MENU_TEXT_COLUMN_TEXT;
    g_at_list_layout.columns.mode[2] = MENU_TEXT_COLUMN_TEXT;
    /* Action rows need the wide window and the second text column. */
    if (bracketed != 0) {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->columns.row_count = 6;
        entry->window.x = 0x8c;
        entry->window.y = 0x30;
        entry->window.w = 0xe8;
        entry->text_image.source.w = 0xe8;
        entry->inner_width = 0xe8;
    } else {
        menu_scroll_text_layout_t* layout = entry->context;

        layout->columns.row_count = 6;
        entry->window.x = 0xbe;
        entry->window.y = 0x30;
        entry->window.w = 0x84;
        entry->text_image.source.w = 0x84;
        entry->inner_width = 0x84;
        g_at_list_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
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

    /* Scroll so the previewed row sits fourth from the top where the list allows. */
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

    g_at_list_layout.columns.text_ids[0] = work->entry_numbers;
    g_at_list_layout.columns.text_ids[2] = work->secondary_values;
    g_at_list_layout.columns.text_colors[0] = work->primary_colors;
    g_at_list_layout.columns.text_ids[1] = work->primary_values;
    g_at_list_layout.columns.text_colors[1] = work->secondary_colors;
    g_at_list_layout.columns.text_colors[2] = work->secondary_colors;
}

/* Thread body started from the ability list: build the AT list preview for the
 * highlighted ability, pulse SCRIPT_TUTORIAL_WAIT_ABILITY_AT_LIST and run it as a
 * scrolling list. */
void at_list_ability_preview_thread(void) {
    at_list_build_ability_preview();
    tutorial_pulse_wait_value(SCRIPT_TUTORIAL_WAIT_ABILITY_AT_LIST);
    ability_list_scrolling_thread();
}
