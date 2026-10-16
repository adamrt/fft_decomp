/* Icon primitives, the auto battle setting, the icon selection loop, ability preview AT lists, narrow text columns,
 * page redraws in a rect and the event's initial repeat counter. */

#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_ai_command.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/main_file.h"
#include "fft/main_sound.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_at_list.h"
#include "fft/menu_command.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_selection.h"
#include "fft/menu_status.h"
#include "fft/menu_window.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Row actions of the auto-battle setting menu (menu entry 0x80166b4c, +0x24). */
static s16 g_menu_auto_battle_row_actions[8] = { 1000, 2, 2, 2, 2, 0, -3, -3 };

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

    param = (menu_entry_t*)thread_get_current_parameter_1();
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
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(records[1]));
    fresh = 0;
    i = 0;
    color = g_menu_auto_battle_option_colors;
    for (;; i++) {
        record = &records[i & 1];
        if (i == 0 || (g_script_event_input & PSX_PAD_CIRCLE)) {
            buffer = menu_build_and_upload_window_frame_image(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            if (((g_script_event_input & PSX_PAD_CIRCLE) && thread_is_previous_running() == 0) || i == 0) {
                selected = cursor;
                if (selected == 0) {
                    before = battle_menu_get_idle_action_menu_id(g_menu_selected_unit.battle_id);
                    stats->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                    g_menu_pending_selection[0] = 0x12;
                    color[0] = 8;
                    color[1] = 4;
                    color[2] = 4;
                    color[3] = 4;
                    color[4] = 4;
                    after = battle_menu_get_idle_action_menu_id(g_menu_selected_unit.battle_id);
                    if (before != after) {
                        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
                        menu_build_idle_action_menu(after);
                    }
                } else {
                    color[0] = 4;
                    color[1] = 0;
                    color[2] = 0;
                    color[3] = 0;
                    color[4] = 0;
                    color[selected] = 8;
                }
                g_menu_auto_battle_row_actions[1] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[2] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[3] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[4] = MENU_COMMAND_ID_AUTO_BATTLE_CONFIRM;
                if (cursor == previous) {
                    g_menu_auto_battle_row_actions[1] = MENU_COMMAND_ID_AUTO_BATTLE_TARGET;
                    g_menu_auto_battle_row_actions[2] = MENU_COMMAND_ID_AUTO_BATTLE_TARGET;
                }
                previous = cursor;
                g_menu_thread_menu_data[MENU_COMMAND_ID_AUTO_BATTLE_TARGET].selected_index = 0;
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
 * battle_action_preview_at_list runs on the main stack for the selected unit and fills the
 * AT list work area in the companion overlay space. The names of the listed units are written
 * into the AT list text section, and the ABILITY_PREVIEW_AT_LIST entry is laid out with
 * number, unit and action columns (the action column and wide window only when an action row
 * is listed), scrolled so the previewed row shows. When the action gets no turn, this
 * thread shows ABILITY_PREVIEW_MESSAGE (text 0x1023) instead and exits there. */
static void menu_build_ability_preview_at_list(void) {
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
    const u16* at_table = g_ai_workspace_ptr->ability_list.ids;

    work = (menu_at_list_work_t*)g_main_heap_companion_overlay_load_address;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_AT_LIST].select_text_table = 0x13;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_AT_LIST].context = &g_menu_at_list_layout;
    text_id = at_table[g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index];
    g_menu_selected_action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    entry = &g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_PREVIEW_AT_LIST];

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
    g_thread_call_target = (void (*)(void))battle_action_preview_at_list;
    turn = thread_call_on_main_stack(stats, action, work->entries);
    /* An item preview marks the first entry as the item action; the loop below names it
     * from the item text. */
    if (staged_item != 0) {
        work->entries[1].unit_code |= AT_ACTION_ENTRY;
        work->entries[1].action_id_low = g_menu_selected_action.item_or_equipment;
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
        stats = battle_unit_get_stats_from_battle_id(slot->unit_code & AT_UNIT_ID_MASK);
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
    g_menu_at_list_layout.columns.mode[0] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_at_list_layout.columns.mode[1] = MENU_TEXT_COLUMN_TEXT;
    g_menu_at_list_layout.columns.mode[2] = MENU_TEXT_COLUMN_TEXT;
    /* Action rows need the wide window and the second text column. */
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

    g_menu_at_list_layout.columns.text_ids[0] = work->entry_numbers;
    g_menu_at_list_layout.columns.text_ids[2] = work->secondary_values;
    g_menu_at_list_layout.columns.text_colors[0] = work->primary_colors;
    g_menu_at_list_layout.columns.text_ids[1] = work->primary_values;
    g_menu_at_list_layout.columns.text_colors[1] = work->secondary_colors;
    g_menu_at_list_layout.columns.text_colors[2] = work->secondary_colors;
}

/* Thread body started from the ability list: build the AT list preview for the
 * highlighted ability, pulse SCRIPT_TUTORIAL_WAIT_ABILITY_AT_LIST and run it as a
 * scrolling list. */
void menu_preview_ability_turn_at_list(void) {
    menu_build_ability_preview_at_list();
    script_pulse_tutorial_wait_value(SCRIPT_TUTORIAL_WAIT_ABILITY_AT_LIST);
    menu_run_scrolling_ability_list_thread();
}

/* Draw the visible rows of a three-column menu text layout (text 8 pixels
 * narrower than the entry). BATTLE appends text 0x5088 to text columns whose
 * skill flag bit 0 is set while the current thread's parameter 4 is nonzero,
 * and its number-glyph columns draw a stacked glyph pair for values 0/1 of an
 * ability whose skill_ct is 0 or 0xFF and text 0x509d for values of 100 and above;
 * WORLD skips entries whose id is 0x7ff. */
void menu_draw_text_columns_narrow(menu_entry_t* entry, s32* row_offset, u8* buffer) {
    menu_text_layout_t* layout = entry->context;
    s32 row;
    s32 column;
    s32 index;
#ifdef FFT_BATTLE
    s16 value;
#else
    /* WORLD's build reaches the stride through a pointer cell. */
    s32* stride_cell = &g_menu_text_state.pen.stride;
#endif

    for (row = 0; row < layout->row_count; row++) {
#ifdef FFT_BATTLE
        g_menu_text_state.pen.stride = entry->inner_width - 8;
#else
        *stride_cell = entry->inner_width - 8;
#endif
        for (column = 0; column < 3; column++) {
            index = row + *row_offset;
            if (layout->mode[column] == MENU_TEXT_COLUMN_HIDDEN) {
                continue;
            }
#ifdef FFT_WORLD
            if ((layout->text_ids[column][index] & 0x7FF) == 0x7FF) {
                continue;
            }
#endif
            menu_set_text_origin(layout->x[column], row * 0x10);
            if (layout->mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                g_menu_text_palette_offset = layout->text_colors[column][index];
            }
            /* A row drawn in palette 4 (the highlighted row) also gets a background of
             * color index 4 in every pixel nibble. */
            g_menu_text_state.pen.color = 0;
            if (g_menu_text_palette_offset == 4) {
                g_menu_text_state.pen.color = 0x44444444;
            }
            if (layout->mode[column] == MENU_TEXT_COLUMN_TEXT) {
                menu_display_text_entry(layout->text_ids[column][index], buffer, &g_menu_text_state.pen);
#ifdef FFT_BATTLE
                if (g_threads[g_thread_current_id].function_parameter_4 != 0
                    && (g_ai_workspace_ptr->ability_list.flags[index] & 1)) {
                    g_menu_text_palette_offset = layout->text_colors[column][index];
                    /* 32-bit x and y, as above. */
                    menu_set_text_origin(layout->x[column] + 0x50, row * 0x10);
                    menu_display_text_entry(0x5088, buffer, &g_menu_text_state.pen);
                }
#endif
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                menu_display_number_entry(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
#ifdef FFT_BATTLE
                if ((column == 2 || (column == 1 && layout->mode[2] == MENU_TEXT_COLUMN_HIDDEN))
                    && layout->text_ids[column][index] >= 0 && layout->text_ids[column][index] < 2
                    && (g_ai_workspace_ptr->ability_list.skill_ct[index] == 0xFF
                        || g_ai_workspace_ptr->ability_list.skill_ct[index] == 0)) {
                    g_menu_text_state.pen.rect.y += 2;
                    menu_draw_stacked_glyph_pair(buffer, &g_menu_text_state.pen);
                } else {
                    value = layout->text_ids[column][index];
                    if (value & 0x7FF) {
                        if ((value & 0x7FF) >= 100) {
                            g_menu_text_palette_offset = 0;
                            g_menu_text_state.pen.rect.x += 3;
                            menu_display_text_entry(0x509D, buffer, &g_menu_text_state.pen);
                        } else {
                            menu_draw_large_number_glyphs(value, 2, buffer, &g_menu_text_state.pen);
                        }
                    }
                }
#else
                menu_draw_large_number_glyphs(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
#endif
            }
            g_menu_text_palette_offset = 0;
            g_menu_text_state.pen.color = 0;
        }
    }
}

#ifdef FFT_BATTLE
/*
 * WORLD twin:
 * world_menu_redraw_text_page_on_scroll_in_rect, plus the mode-2 buffer
 * placement after the event overlay.
 */

/* Scrolls the row window of a three-column menu page on Square+Up/Down (or
 * on the running thread's redraw request), redraws the page into an image
 * buffer and uploads it to the caller-supplied rectangle (x and w in
 * framebuffer pixels, divided by four; the image row is fixed at y = 0x80).
 * At g_battle_menu_scroll_list_depth 2 the buffer is placed 0x2000 bytes past the companion
 * overlay load address instead of being allocated. Returns that buffer when the page was
 * redrawn, else 0. */
u8* battle_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32 unused, RECT* area) {
    menu_text_layout_t* layout = entry->context;
    u32* pressed;
    u32* held;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    u8* buffer;

    pressed = script_get_controller_input_pointer(0);
    held = script_get_controller_input_pointer(1);
    rect.x = (area->x / 4) + entry->vram_origin.x;
    rect.y = 0x80;
    rect.w = area->w / 4;
    rect.h = area->h;
    changed = 0;
    if (g_menu_input_mode == MENU_INPUT_PLAYER
        && ((*held & PSX_PAD_SQUARE)
            || g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] != 0)) {
        /* Page up or down by one screen of rows, clamped to the list. */
        if ((*pressed & PSX_PAD_UP) && *row_offset != 0) {
            offset = *row_offset - layout->row_count;
            *row_offset = offset;
            if (offset < 0) {
                *row_offset = 0;
            }
            g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
            changed = 1;
        } else if ((*pressed & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
            offset = layout->row_count + *row_offset;
            *row_offset = offset;
            if (layout->hidden_rows < offset) {
                *row_offset = layout->hidden_rows;
            }
            g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
            changed = 1;
        } else if (g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] != 0) {
            changed = 1;
            g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] = 0;
        }
        if (changed != 0) {
            if (g_battle_menu_scroll_list_depth == 2) {
                buffer = g_main_heap_companion_overlay_load_address + 0x2000;
            } else {
                buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
            }
            menu_clear_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
            menu_draw_text_columns_narrow(entry, row_offset, buffer);
            *redraw = 0;
            LoadImage(&rect, buffer);
            /* Clear the two VRAM rows above the image, wrapping within its 256-row page. */
            mem_copy_bytes_nonempty(&clear, &rect, sizeof(clear));
            clear.h = 2;
            clear.x = rect.x;
            clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
            clear.w = rect.w;
            ClearImage(&clear, 0, 0, 0);
            return buffer;
        }
    }
    return 0;
}
#endif

#ifdef FFT_WORLD
/* Sibling of world_screen_menu_redraw_text_page_on_scroll that draws through
 * menu_draw_text_columns_narrow and takes the image rectangle from a caller-supplied
 * RECT (x and w in framebuffer pixels, divided by four; the image row is
 * fixed at y = 0x80). Pages the row window on Square+Up/Down (or on the running
 * thread's redraw request), redraws the page into a freshly allocated image
 * buffer and uploads it. Returns that buffer when the page was redrawn, else 0. */
u8* world_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area) {
    menu_text_layout_t* layout = entry->context;
    u32* pressed;
    u32* held;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    u8* buffer;

    pressed = script_get_controller_input_pointer(0);
    held = script_get_controller_input_pointer(1);
    rect.x = (area->x / 4) + entry->vram_origin.x;
    rect.y = 0x80;
    rect.w = area->w / 4;
    rect.h = area->h;
    changed = 0;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        return 0;
    }
    if (!(*held & PSX_PAD_SQUARE)
        && g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] == 0) {
        return 0;
    }
    /* Page up or down by one screen of rows, clamped to the list. */
    if ((*pressed & PSX_PAD_UP) && *row_offset != 0) {
        offset = *row_offset - layout->row_count;
        *row_offset = offset;
        if (offset < 0) {
            *row_offset = 0;
        }
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
        changed = 1;
    } else if ((*pressed & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
        offset = layout->row_count + *row_offset;
        *row_offset = offset;
        if (layout->hidden_rows < offset) {
            *row_offset = layout->hidden_rows;
        }
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
        changed = 1;
    } else if (g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] != 0) {
        changed = 1;
        g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] = 0;
    }
    if (changed == 0) {
        return 0;
    }
    buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    menu_clear_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    menu_draw_text_columns_narrow(entry, row_offset, buffer);
    *redraw = 0;
    LoadImage(&rect, buffer);
    /* Clear the two VRAM rows above the image, wrapping within its 256-row page. */
    mem_copy_bytes_nonempty(&clear, &rect, sizeof(clear));
    clear.h = 2;
    clear.x = rect.x;
    clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
    clear.w = rect.w;
    ClearImage(&clear, 0, 0, 0);
    return buffer;
}
#endif

/* Frames the current repeatable buttons have been held unchanged (0 after a change);
 * the scrolling lists pace and repeat their scrolling by it. */
u32 script_get_event_initial_repeat_counter(void) {
    return g_script_event_initial_repeat_counter;
}
