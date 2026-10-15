/* Icon primitives, the auto battle setting, ability preview AT lists, text columns, timed icon threads, panel fades,
 * overlay reservation, companion file requests, display conditions, the dark screen and spell quote threads, and the
 * openers for the option, BUNIT, help, JOBSTTS, EQUIP and DEBUGCHR menus. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/event_debugchr.h"
#include "fft/event_etc.h"
#include "fft/event_helpmenu.h"
#include "fft/event_option.h"
#include "fft/event_require.h"
#include "fft/event_small.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static void menu_draw_text_columns_narrow(menu_entry_t* entry, s32* row_offset, u8* buffer);

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

/* Build the selected ability/item AT preview and its three-column menu. */
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
    g_thread_call_target = (void (*)(void))battle_action_preview_at_list;
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
static void menu_preview_ability_turn_at_list(void) {
    menu_build_ability_preview_at_list();
    script_pulse_tutorial_wait_value(0xFA);
    menu_run_scrolling_ability_list_thread_variant();
}

/* Draw the visible rows of a three-column menu text layout (text 8 pixels
 * narrower than the entry). BATTLE appends text 0x5088 to text columns whose
 * skill flag bit 0 is set while the current thread's fourth 32-bit value is
 * nonzero, and its number-glyph columns draw a special glyph for values 0/1
 * without a target and text 0x509d for values of 100 and above; WORLD skips
 * entries whose id is 0x7ff. */
static void menu_draw_text_columns_narrow(menu_entry_t* entry, s32* row_offset, u8* buffer) {
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
                            menu_draw_number_glyphs(value, 2, buffer, &g_menu_text_state.pen);
                        }
                    }
                }
#else
                menu_draw_number_glyphs(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
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
 * In mode 2 of g_battle_menu_scroll_list_depth the buffer is placed 0x2000 bytes past the event
 * overlay instead of being allocated. Returns that buffer when the page was
 * redrawn. */
static u8* battle_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32 unused, RECT* area) {
    menu_text_layout_t* layout = entry->context;
    u32* input;
    u32* input2;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    u8* buffer;

    input = script_get_controller_input_pointer(0);
    input2 = script_get_controller_input_pointer(1);
    rect.x = (area->x / 4) + entry->vram_origin.x;
    rect.y = 0x80;
    rect.w = area->w / 4;
    rect.h = area->h;
    changed = 0;
    if (g_menu_input_mode == MENU_INPUT_PLAYER
        && ((*input2 & PSX_PAD_SQUARE) || g_threads[g_thread_current_id].task_words[4] != 0)) {
        if ((*input & PSX_PAD_UP) && *row_offset != 0) {
            offset = *row_offset - layout->row_count;
            *row_offset = offset;
            if (offset < 0) {
                *row_offset = 0;
            }
            g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
            changed = 1;
        } else if ((*input & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
            offset = layout->row_count + *row_offset;
            *row_offset = offset;
            if (layout->hidden_rows < offset) {
                *row_offset = layout->hidden_rows;
            }
            g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
            changed = 1;
        } else if (g_threads[g_thread_current_id].task_words[4] != 0) {
            changed = 1;
            g_threads[g_thread_current_id].task_words[4] = 0;
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
 * world_draw_menu_text_columns_narrow and takes the image rectangle from a caller-supplied
 * RECT (x and w in framebuffer pixels, divided by four; the image row is
 * fixed at y = 0x80). Scrolls the row window on L1/R1 (or on the running
 * thread's redraw request), redraws the page into a freshly allocated image
 * buffer and uploads it. Returns that buffer when the page was redrawn. */
static u8* world_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area) {
    menu_text_layout_t* layout = entry->context;
    u32* input0;
    u32* input1;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    u8* buffer;

    input0 = script_get_controller_input_pointer(0);
    input1 = script_get_controller_input_pointer(1);
    rect.x = (area->x / 4) + entry->vram_origin.x;
    rect.y = 0x80;
    rect.w = area->w / 4;
    rect.h = area->h;
    changed = 0;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        return 0;
    }
    if (!(*input1 & PSX_PAD_SQUARE) && g_threads[g_thread_current_id].task_words[4] == 0) {
        return 0;
    }
    if ((*input0 & PSX_PAD_UP) && *row_offset != 0) {
        offset = *row_offset - layout->row_count;
        *row_offset = offset;
        if (offset < 0) {
            *row_offset = 0;
        }
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
        changed = 1;
    } else if ((*input0 & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
        offset = layout->row_count + *row_offset;
        *row_offset = offset;
        if (layout->hidden_rows < offset) {
            *row_offset = layout->hidden_rows;
        }
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
        changed = 1;
    } else if (g_threads[g_thread_current_id].task_words[4] != 0) {
        changed = 1;
        g_threads[g_thread_current_id].task_words[4] = 0;
    }
    if (changed == 0) {
        return 0;
    }
    buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    menu_clear_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    menu_draw_text_columns_narrow(entry, row_offset, buffer);
    *redraw = 0;
    LoadImage(&rect, buffer);
    mem_copy_bytes_nonempty(&clear, &rect, sizeof(clear));
    clear.h = 2;
    clear.x = rect.x;
    clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
    clear.w = rect.w;
    ClearImage(&clear, 0, 0, 0);
    return buffer;
}
#endif

u32 script_get_event_initial_repeat_counter(void) {
    return g_script_event_initial_repeat_counter;
}

#ifdef FFT_BATTLE
/*
 * Scrolling three-column ability list thread, the BATTLE twin of
 * menu_run_scrolling_ability_list_thread_variant (WORLD 0x800edaa8).
 *
 * Beyond the WORLD version, it nests through g_battle_menu_scroll_list_depth: nested lists keep
 * their packet pages at the event overlay + 0x1000 (level 2 also places the
 * text buffer at + 0x2000), and the outermost list saves the CLUT at
 * g_battle_menu_ability_list_clut_rect as the first of five CLUTs in a buffer, derives the other four from it
 * and blinks CLUTs 3/4 while it runs. Square/Circle on a row whose text is not
 * 0x7000 starts battle_menu_preview_ability_turn_at_list in the previous
 * thread slot (with entry 52 of g_menu_thread_menu_data) and blinks CLUTs 1/2 until
 * it finishes.
 *
 * Layout notes: the blink loop's exit block sits inside the loop as
 * `if (...) { ...; break; }` so loop.c moves it in front of the else-if test,
 * as in the target; `base` is copied in each branch so cross-jumping leaves
 * one spill store at the join; and the blink counter is cleared after the
 * thread setup so the scheduler keeps the entry address load ahead of the
 * thread-id load.
 */
void menu_run_scrolling_ability_list_thread_variant(void) {
    menu_text_window_t window;
    RECT image_rect;
    RECT texture_window;
    SPRT icons0[2];
    SPRT icons1[2];
    SPRT icons2[2];
    RECT clear;
    s32 row_offset;
    SPRT* frame;
    s16 toggle;
    s32 ring;
    s32 cursor;
    menu_list_page_t* base;
    menu_scroll_text_layout_t* layout;
    s32 scroll_accum;
    s32 scroll_delta;
    s32 hold_time;
    s32 redraw_pending;
    s32 row_buffer_live;
    menu_entry_t* entry;
    menu_list_page_t* page;
    u8* buffer;
    s32 i;
    s32 column;
    s32 index;
    s16 value;
    s32 step;
    s32 top;
    s32 arrow_u;
    s32 arrow_y;
    s32 repeat;
    s32 span;
    u16* clut;
    u16* saved_clut;

    saved_clut = g_menu_scratch_buffer.clut_save;
    row_buffer_live = 0;
    hold_time = 0;
    if (g_battle_menu_scroll_list_depth != 0) {
        /* Nested list: page pair in the event overlay area, 0x1000 bytes in. */
        page = (menu_list_page_t*)(g_main_heap_companion_overlay_load_address + 0x1000);
        base = page;
    } else {
        /* Outermost list: page pair at +0x388 of the current subsystem workspace. */
        page = g_ai_workspace_ptr->list_pages;
        base = page;
    }
    toggle = 0;
    g_battle_menu_scroll_list_depth++;
    entry = (menu_entry_t*)thread_get_current_parameter_1();
    layout = entry->context;
    entry->window.h = layout->columns.row_count * 16 + 0x1C;
    entry->inner_height = layout->columns.row_count * 16 + 0x1C;
    entry->text_image.height = layout->columns.row_count * 16 + 0x1C;
    window.rect.x = 0x108 - entry->inner_width;
    window.rect.y = 0xFE;
    window.x_screen_offset = 8;
    window.y_screen_offset = 0xC;
    window.rect.w = entry->inner_width - 8;
    window.rect.h = entry->window.h - 0x1A;
    clear.x = window.rect.x / 4 + entry->vram_origin.x;
    clear.y = (entry->vram_origin.y & 0xFF00) + 0xFE;
    clear.w = window.rect.w / 4;
    clear.h = 2;
    ClearImage(&clear, 0, 0, 0);
    texture_window.x = 0;
    texture_window.y = 0x80;
    texture_window.w = 0x100;
    texture_window.h = 0x80;
    for (i = 0; i < 2; i++) {
        base[i].icons.icons[0] = &icons0[i];
        base[i].icons.icons[1] = &icons1[i];
        base[i].icons.icons[2] = &icons2[i];
        menu_build_window_sprites(&image_rect, entry, &base[i].icons);
    }
    SetDrawMode(&page->icon_mode, 0, 0, GetTPage(0, 2, 0x3C0, 0x100), &g_gfx_texture_window);
    SetDrawMode(&page->text_mode, 1, 0, GetTPage(0, 1, entry->vram_origin.x & 0xFFC0, entry->vram_origin.y & 0xFF00),
        &texture_window);
    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        menu_init_sprt(&page->arrows[i]);
        page->arrows[i].clut = 0x7D7C;
        page->arrows[i].w = 8;
        page->arrows[i].h = 0x10;
        page->arrows[i].u0 = arrow_u;
        page->arrows[i].v0 = 0;
        page->arrows[i].x0 = entry->window.x + entry->window.w - 0xF;
        arrow_y = i * (entry->window.h - 0x32) + 0x10;
        page->arrows[i].y0 = entry->window.y + arrow_y;
        arrow_u += 8;
    }
    menu_init_sprt(&page->thumb);
    page->thumb.u0 = 0x10;
    page->thumb.v0 = 0x10;
    page->thumb.w = 8;
    page->thumb.h = 8;
    page->thumb.clut = 0x7D7C;
    menu_init_sprt(&page->frame);
    gfx_init_image_loading(&page->frame, &entry->vram_origin, &entry->window, (const image_load_parameters_t*)&window);
    menu_init_sprite_array(page->arrow_marks, 2, 0x7C3C);
    page->arrow_marks[0].w = 6;
    page->arrow_marks[0].h = 0xC;
    page->arrow_marks[1].w = 4;
    page->arrow_marks[1].h = 4;
    page->arrow_marks[0].u0 = 0xE8;
    page->arrow_marks[1].u0 = 0xE8;
    page->arrow_marks[0].v0 = 0;
    page->arrow_marks[0].x0 = page->arrows[0].x0 - 1;
    page->arrow_marks[0].y0 = page->arrows[0].y0 + 3;
    page->arrow_marks[1].v0 = 0xC;
    page->arrow_marks[1].x0 = page->arrows[1].x0;
    page->arrow_marks[1].y0 = page->arrows[1].y0 + 0xE;
    menu_build_and_upload_window_frame_image(
        entry->inner_width, entry->inner_height, &image_rect, MENU_WINDOW_FRAME_ALTERNATE_UPLOAD);
    if (g_battle_menu_scroll_list_depth == 2) {
        buffer = g_main_heap_companion_overlay_load_address + 0x2000;
    } else {
        StoreImage(&g_battle_menu_ability_list_clut_rect, (u32*)saved_clut);
        buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    }
    menu_clear_render_buffer(buffer, window.rect.w * window.rect.h / 2);
    row_offset = layout->row_offset;
    if (layout->columns.hidden_rows < row_offset) {
        row_offset = layout->columns.hidden_rows;
    }
    cursor = entry->selected_index - row_offset;
    if (cursor >= layout->columns.row_count) {
        row_offset = 0;
        cursor = 0;
    }
    menu_draw_text_columns_narrow(entry, &row_offset, buffer);
    redraw_pending = 1;
    ring = 0;
    image_rect.x = window.rect.x / 4 + entry->vram_origin.x;
    image_rect.y = 0x80;
    image_rect.w = window.rect.w / 4;
    image_rect.h = window.rect.h;
    LoadImage(&image_rect, (u32*)buffer);
    mem_copy_bytes_nonempty(&base[1], base, sizeof(menu_list_page_t));
    for (i = 0; i < 2; i++) {
        base[i].icons.icons[0] = &icons0[i];
        base[i].icons.icons[1] = &icons1[i];
        base[i].icons.icons[2] = &icons2[i];
    }
    i = 0;
    scroll_accum = 0;
    scroll_delta = 0;
    for (;;) {
        page = &base[i & 1];
        frame = &page->frame;
        SetSemiTrans(&frame, 1);
        if (scroll_delta == 0 && menu_can_accept_input() != 0 && i != 0) {
            if (layout->columns.mode[0] == MENU_TEXT_COLUMN_TEXT && layout->columns.mode[1] == MENU_TEXT_COLUMN_TEXT) {
                if (g_script_event_input & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
                    toggle ^= 1;
                }
                layout->columns.text_ids[2] = &toggle;
            } else if ((g_script_event_input & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) && g_battle_menu_scroll_list_depth == 1
                && layout->columns.mode[1] != MENU_TEXT_COLUMN_HIDDEN && i != 0
                && layout->columns.text_ids[0][entry->selected_index] != 0x7000) {
                thread_yield();
                sound_queue_sfx_cursor_move();
                thread_start(g_thread_current_id - 1, menu_preview_ability_turn_at_list);
                thread_set_parameters(g_thread_current_id - 1,
                    (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_PREVIEW_AT_LIST], 0, 0);
                column = 0;
                while (1) {
                    if (thread_is_running_asm(g_thread_current_id - 1) == 0) {
                        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 48));
                        break;
                    }
                    if (column % 30 < 24) {
                        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 16));
                    } else {
                        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 32));
                    }
                    column++;
                    thread_yield();
                }
            }
            if ((g_script_event_input & PSX_PAD_UP)
                && (!(g_script_event_current_input & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != 0) {
                    cursor--;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset != 0) {
                    scroll_delta = -1;
                    sound_queue_sfx_cursor_move();
                } else if (layout->columns.hidden_rows == 0) {
                    cursor = layout->columns.row_count - 1;
                    sound_queue_sfx_cursor_move();
                }
            } else if ((g_script_event_input & PSX_PAD_DOWN)
                && (!(g_script_event_current_input & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != layout->columns.row_count - 1) {
                    cursor++;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset != layout->columns.hidden_rows) {
                    scroll_delta++;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset == 0) {
                    cursor = 0;
                    sound_queue_sfx_cursor_move();
                }
            } else {
                hold_time = 0;
            }
            if (scroll_delta != 0) {
                if (scroll_delta > 0) {
                    image_rect.y = (((ring + layout->columns.row_count) * 16) & 0x7F) + entry->vram_origin.y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0x7F) + entry->vram_origin.y;
                }
                image_rect.h = 0x10;
                image_rect.y += 0x80;
                row_buffer_live = 1;
                buffer = menu_alloc_memory(window.rect.w * 8);
                menu_clear_render_buffer(buffer, window.rect.w * 8);
                g_menu_text_state.pen.stride = window.rect.w;
                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + scroll_delta;
                    menu_set_text_origin(layout->columns.x[column], 0);
                    if (layout->columns.mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                    }
                    g_menu_text_state.pen.color = 0;
                    if (g_menu_text_palette_offset == 4) {
                        g_menu_text_state.pen.color = 0x44444444;
                    }
                    if (layout->columns.mode[column] == MENU_TEXT_COLUMN_TEXT) {
                        menu_display_text_entry(
                            layout->columns.text_ids[column][index], buffer, &g_menu_text_state.pen);
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                        if (g_thread_contexts[g_thread_current_id].function_parameter_4 != 0
                            && (g_ai_workspace_ptr->ability_list.flags[index] & 1)) {
                            menu_set_text_origin(layout->columns.x[column] + 0x50, 0);
                            menu_display_text_entry(0x5088, buffer, &g_menu_text_state.pen);
                        }
                    } else if (layout->columns.mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                        menu_display_number_entry(
                            layout->columns.text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
                    } else if (layout->columns.mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                        if ((column == 2 || (column == 1 && layout->columns.mode[2] == MENU_TEXT_COLUMN_HIDDEN))
                            && layout->columns.text_ids[column][index] >= 0
                            && layout->columns.text_ids[column][index] < 2
                            && (g_ai_workspace_ptr->ability_list.skill_ct[index] == 0xFF
                                || g_ai_workspace_ptr->ability_list.skill_ct[index] == 0)) {
                            g_menu_text_state.pen.rect.y += 2;
                            menu_draw_stacked_glyph_pair(buffer, &g_menu_text_state.pen);
                        } else {
                            value = layout->columns.text_ids[column][index];
                            if (value & 0x7FF) {
                                if ((value & 0x7FF) >= 100) {
                                    g_menu_text_palette_offset = 0;
                                    g_menu_text_state.pen.rect.x += 3;
                                    menu_display_text_entry(0x509D, buffer, &g_menu_text_state.pen);
                                } else {
                                    menu_draw_number_glyphs(value, 2, buffer, &g_menu_text_state.pen);
                                }
                            }
                        }
                    }
                }
                g_menu_text_palette_offset = 0;
                g_menu_text_state.pen.color = 0;
                LoadImage(&image_rect, (u32*)buffer);
                clear.h = 2;
                clear.x = image_rect.x;
                clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
                clear.w = image_rect.w;
                ClearImage(&clear, 0, 0, 0);
            }
        }
        thread_yield();
        if (i == 0) {
            for (column = 0; column < 16; column++) {
                clut = &saved_clut[column];
                clut[16] = clut[0];
                clut[32] = clut[0];
                clut[48] = clut[0];
                clut[64] = clut[0];
                if (column >= 4 && column < 8) {
                    clut[32] = 0x8000;
                    clut[16] = clut[-4];
                } else if (column >= 8) {
                    clut[64] = clut[56];
                }
            }
        }
        if (g_battle_menu_scroll_list_depth == 1) {
            if (i % 30 < 24) {
                LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 48));
            } else {
                LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)(saved_clut + 64));
            }
        }
        if (redraw_pending != 0) {
            menu_free_memory(buffer);
            redraw_pending = 0;
        }
        if (scroll_delta == 0 && menu_can_accept_input() != 0) {
            buffer
                = battle_menu_redraw_text_page_on_scroll_in_rect(entry, &row_offset, &ring, (s32)&cursor, &window.rect);
            redraw_pending = (s32)buffer;
            ClearImage(&clear, 0, 0, 0);
        }
        entry->selected_index = cursor + row_offset;
        layout->row_offset = row_offset;
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        menu_select_icon_cluts(&page->icons.base);
        menu_select_icon_sprite_clut(page->icons.icons[0]);
        menu_select_icon_sprite_clut(page->icons.icons[1]);
        menu_select_icon_sprite_clut(page->icons.icons[2]);
        if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }
        menu_update_icon_cursor_sprites(entry, &page->icons.base, i, cursor);
        if (layout->columns.hidden_rows != 0 && thread_is_previous_running() == 0) {
            if (row_offset != 0) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->columns.hidden_rows) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0
                = top + span * entry->selected_index / (layout->columns.row_count + layout->columns.hidden_rows) - 2;
            gfx_draw_or_append_gpu_primitive(&page->thumb);
        }
        if (g_battle_menu_scroll_list_depth < 2 && scroll_delta == 0) {
            menu_handle_action(entry, cursor);
        }
        menu_handle_cancel_input(entry);
        if ((g_script_event_speed == 1 && !(script_get_event_initial_repeat_counter() & 1))
            || g_script_event_speed == 2) {
            if (hold_time < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step;
            } else {
                step = g_main_menu_scroll_fast_step;
            }
            scroll_accum += scroll_delta * (step << 1);
        }
        hold_time += g_script_event_speed;
        if (scroll_accum < -0xF || scroll_accum > 0xF) {
            ring += scroll_delta;
            row_offset += scroll_delta;
            repeat = script_get_event_initial_repeat_counter();
            scroll_accum = 0;
            scroll_delta = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_script_event_speed) {
                g_script_event_input |= g_script_event_current_input & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
            menu_free_memory(buffer);
            row_buffer_live = 0;
        }
        frame->v0 = (ring * 16 + scroll_accum - 2) & 0x7F;
        if (i * g_script_event_speed < 12) {
            gfx_init_image_loading(
                &page->frame, &entry->vram_origin, &entry->window, (const image_load_parameters_t*)&window);
            menu_zoom_cursor_frame(entry, frame, i * g_script_event_speed);
        }
        if (layout->columns.mode[1] == MENU_TEXT_COLUMN_TEXT && toggle == 1) {
            {
                menu_text_layout_t* columns = entry->context;

                page->icons.base.sprites[1].x0 += columns->x[1];
                columns = entry->context;
                page->icons.base.sprites[2].x0 += columns->x[1];
            }
        }
        page->icons.base.sprites[1].y0 += 4;
        page->icons.base.sprites[2].y0 += 4;
        if (entry->header_id >= 5) {
            gfx_draw_or_append_gpu_primitive(page->icons.icons[0]);
            gfx_draw_or_append_gpu_primitive(page->icons.icons[1]);
            if (g_thread_contexts[g_thread_current_id].function_parameter_4 != 0) {
                gfx_draw_or_append_gpu_primitive(page->icons.icons[2]);
            }
        }
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[3]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[1]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[2]);
        gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        gfx_draw_or_append_gpu_primitive(frame);
        gfx_draw_or_append_gpu_primitive(&page->text_mode);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.draw_mode);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[0]);
        gfx_draw_or_append_gpu_primitive(&page->icons);
        i++;
    }
    if (g_battle_menu_scroll_list_depth != 2) {
        LoadImage(&g_battle_menu_ability_list_clut_rect, (u32*)saved_clut);
    }
    if (row_buffer_live != 0) {
        menu_free_memory(buffer);
    }
    thread_yield();
    g_battle_menu_scroll_list_depth--;
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}
#endif

#ifdef FFT_WORLD
/*
 * Scrolling three-column ability list thread (the WORLD twin of BATTLE
 * battle_menu_run_scrolling_ability_list_thread, reached from menu_preview_ability_turn_at_list).
 *
 * Draws the visible rows into a text image, then runs the input loop on two
 * alternating packet pages. Cursor moves past either end start a one-row
 * scroll whose new row is drawn into the image ring at row `ring * 16`;
 * world_menu_redraw_text_page_on_scroll_in_rect handles page scrolling, and
 * the scroll-bar thumb tracks entry->selected_index. The frame sprite pointer is
 * passed to SetSemiTrans by address, as in the target, which keeps it on the
 * stack.
 */
void menu_run_scrolling_ability_list_thread_variant(void) {
    menu_text_window_t window;
    RECT image_rect;
    RECT texture_window;
    SPRT icons0[2];
    SPRT icons1[2];
    SPRT icons2[2];
    RECT clear;
    s32 row_offset;
    SPRT* frame;
    s16 toggle;
    s32 ring;
    s32 cursor;
    menu_list_page_t* base;
    menu_scroll_text_layout_t* layout;
    s32 accum;
    s32 delta;
    s32 timer;
    s32 redraw_pending;
    s32 row_buffer_live;
    menu_entry_t* entry;
    menu_list_page_t* page;
    u8* buffer;
    s32 i;
    s32 column;
    s32 index;
    s16 value;
    s32 step;
    s32 top;
    s32 arrow_u;
    s32 arrow_y;
    s32 repeat;
    s32 span;

    toggle = 0;
    page = g_world_ability_list_pages[g_thread_current_id];
    entry = (menu_entry_t*)thread_get_current_parameter_1();
    layout = entry->context;
    entry->window.h = layout->columns.row_count * 16 + 0x1C;
    entry->inner_height = layout->columns.row_count * 16 + 0x1C;
    row_buffer_live = 0;
    entry->text_image.height = layout->columns.row_count * 16 + 0x1C;
    window.rect.x = 0x108 - entry->inner_width;
    window.rect.y = 0xFE;
    window.x_screen_offset = 8;
    window.y_screen_offset = 0xC;
    timer = 0;
    window.rect.w = entry->inner_width - 8;
    window.rect.h = entry->window.h - 0x1A;
    base = page;
    clear.x = window.rect.x / 4 + entry->vram_origin.x;
    clear.y = (entry->vram_origin.y & 0xFF00) + 0xFE;
    clear.w = window.rect.w / 4;
    clear.h = 2;
    ClearImage(&clear, 0, 0, 0);
    texture_window.x = 0;
    texture_window.y = 0x80;
    texture_window.w = 0x100;
    texture_window.h = 0x80;
    for (i = 0; i < 2; i++) {
        page[i].icons.icons[0] = &icons0[i];
        page[i].icons.icons[1] = &icons1[i];
        page[i].icons.icons[2] = &icons2[i];
        menu_build_window_sprites(&image_rect, entry, &page[i].icons);
    }
    SetDrawMode(&page->icon_mode, 0, 0, GetTPage(0, 2, 0x3C0, 0x100), &g_gfx_texture_window);
    SetDrawMode(&page->text_mode, 1, 0, GetTPage(0, 1, entry->vram_origin.x & 0xFFC0, entry->vram_origin.y & 0xFF00),
        &texture_window);
    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        menu_init_sprt(&page->arrows[i]);
        page->arrows[i].clut = 0x7D7C;
        page->arrows[i].w = 8;
        page->arrows[i].h = 0x10;
        page->arrows[i].u0 = arrow_u;
        page->arrows[i].v0 = 0;
        page->arrows[i].x0 = entry->window.x + entry->window.w - 0xF;
        arrow_y = i * (entry->window.h - 0x32) + 0x10;
        page->arrows[i].y0 = entry->window.y + arrow_y;
        arrow_u += 8;
    }
    menu_init_sprt(&page->thumb);
    page->thumb.u0 = 0x10;
    page->thumb.v0 = 0x10;
    page->thumb.w = 8;
    page->thumb.h = 8;
    page->thumb.clut = 0x7D7C;
    menu_init_sprt(&page->frame);
    gfx_init_image_loading(&page->frame, &entry->vram_origin, &entry->window, (const image_load_parameters_t*)&window);
    menu_init_sprite_array(page->arrow_marks, 2, 0x7C3C);
    page->arrow_marks[0].w = 6;
    page->arrow_marks[0].h = 0xC;
    page->arrow_marks[1].w = 4;
    page->arrow_marks[1].h = 4;
    page->arrow_marks[0].u0 = 0xE8;
    page->arrow_marks[1].u0 = 0xE8;
    page->arrow_marks[0].v0 = 0;
    page->arrow_marks[0].x0 = page->arrows[0].x0 - 1;
    page->arrow_marks[0].y0 = page->arrows[0].y0 + 3;
    page->arrow_marks[1].v0 = 0xC;
    page->arrow_marks[1].x0 = page->arrows[1].x0;
    page->arrow_marks[1].y0 = page->arrows[1].y0 + 0xE;
    menu_build_and_upload_window_frame_image(
        entry->inner_width, entry->inner_height, &image_rect, MENU_WINDOW_FRAME_ALTERNATE_UPLOAD);
    buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    menu_clear_render_buffer(buffer, window.rect.w * window.rect.h / 2);
    row_offset = layout->row_offset;
    if (layout->columns.hidden_rows < row_offset) {
        row_offset = layout->columns.hidden_rows;
    }
    cursor = entry->selected_index - row_offset;
    if (cursor >= layout->columns.row_count) {
        row_offset = 0;
        cursor = 0;
    }
    menu_draw_text_columns_narrow(entry, &row_offset, buffer);
    redraw_pending = 1;
    ring = 0;
    image_rect.x = window.rect.x / 4 + entry->vram_origin.x;
    image_rect.y = 0x80;
    image_rect.w = window.rect.w / 4;
    image_rect.h = window.rect.h;
    LoadImage(&image_rect, (u32*)buffer);
    mem_copy_bytes_nonempty(&base[1], base, sizeof(menu_list_page_t));
    for (i = 0; i < 2; i++) {
        base[i].icons.icons[0] = &icons0[i];
        base[i].icons.icons[1] = &icons1[i];
        base[i].icons.icons[2] = &icons2[i];
    }
    i = 0;
    accum = 0;
    delta = 0;
    for (;;) {
        page = &base[i & 1];
        frame = &page->frame;
        SetSemiTrans(&frame, 1);
        if (delta == 0 && menu_can_accept_input() != 0 && i != 0) {
            if (layout->columns.mode[0] == MENU_TEXT_COLUMN_TEXT && layout->columns.mode[1] == MENU_TEXT_COLUMN_TEXT) {
                if (g_script_event_input & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
                    toggle ^= 1;
                }
                layout->columns.text_ids[2] = &toggle;
            }
            if ((g_script_event_input & PSX_PAD_UP)
                && (!(g_script_event_current_input & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != 0) {
                    cursor--;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset != 0) {
                    delta = -1;
                    sound_queue_sfx_cursor_move();
                } else if (layout->columns.hidden_rows == 0) {
                    cursor = layout->columns.row_count - 1;
                    sound_queue_sfx_cursor_move();
                }
            } else if ((g_script_event_input & PSX_PAD_DOWN)
                && (!(g_script_event_current_input & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != layout->columns.row_count - 1) {
                    cursor++;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset != layout->columns.hidden_rows) {
                    delta++;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset == 0) {
                    cursor = 0;
                    sound_queue_sfx_cursor_move();
                }
            } else {
                timer = 0;
            }
            if (delta != 0) {
                if (delta > 0) {
                    image_rect.y = (((ring + layout->columns.row_count) * 16) & 0x7F) + entry->vram_origin.y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0x7F) + entry->vram_origin.y;
                }
                image_rect.h = 0x10;
                image_rect.y += 0x80;
                row_buffer_live = 1;
                buffer = menu_alloc_memory(window.rect.w * 8);
                menu_clear_render_buffer(buffer, window.rect.w * 8);
                g_menu_text_state.pen.stride = window.rect.w;
                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + delta;
                    menu_set_text_origin(layout->columns.x[column], 0);
                    if (layout->columns.mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                    }
                    g_menu_text_state.pen.color = 0;
                    if (g_menu_text_palette_offset == 4) {
                        g_menu_text_state.pen.color = 0x44444444;
                    }
                    if (layout->columns.mode[column] == MENU_TEXT_COLUMN_TEXT) {
                        menu_display_text_entry(
                            layout->columns.text_ids[column][index], buffer, &g_menu_text_state.pen);
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                        if (g_thread_contexts[g_thread_current_id].function_parameter_4 != 0
                            && (g_ai_workspace_ptr->ability_list.flags[index] & 1)) {
                            menu_set_text_origin(layout->columns.x[column] + 0x50, 0);
                            menu_display_text_entry(0x5088, buffer, &g_menu_text_state.pen);
                        }
                    } else if (layout->columns.mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                        menu_display_number_entry(
                            layout->columns.text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
                    } else if (layout->columns.mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                        if ((column == 2 || (column == 1 && layout->columns.mode[2] == MENU_TEXT_COLUMN_HIDDEN))
                            && layout->columns.text_ids[column][index] >= 0
                            && layout->columns.text_ids[column][index] < 2
                            && (g_ai_workspace_ptr->ability_list.skill_ct[index] == 0xFF
                                || g_ai_workspace_ptr->ability_list.skill_ct[index] == 0)) {
                            g_menu_text_state.pen.rect.y += 2;
                            menu_draw_stacked_glyph_pair(buffer, &g_menu_text_state.pen);
                        } else {
                            value = layout->columns.text_ids[column][index];
                            if (value & 0x7FF) {
                                if ((value & 0x7FF) >= 100) {
                                    g_menu_text_palette_offset = 0;
                                    g_menu_text_state.pen.rect.x += 3;
                                    menu_display_text_entry(0x509D, buffer, &g_menu_text_state.pen);
                                } else {
                                    menu_draw_number_glyphs(value, 2, buffer, &g_menu_text_state.pen);
                                }
                            }
                        }
                    }
                }
                g_menu_text_palette_offset = 0;
                g_menu_text_state.pen.color = 0;
                LoadImage(&image_rect, (u32*)buffer);
                clear.h = 2;
                clear.x = image_rect.x;
                clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
                clear.w = image_rect.w;
                ClearImage(&clear, 0, 0, 0);
            }
        }
        thread_yield();
        if (redraw_pending != 0) {
            menu_free_memory(buffer);
            redraw_pending = 0;
        }
        if (delta == 0 && menu_can_accept_input() != 0) {
            buffer = world_menu_redraw_text_page_on_scroll_in_rect(entry, &row_offset, &ring, &cursor, &window.rect);
            redraw_pending = (s32)buffer;
            ClearImage(&clear, 0, 0, 0);
        }
        entry->selected_index = cursor + row_offset;
        layout->row_offset = row_offset;
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        menu_select_icon_cluts(&page->icons.base);
        menu_select_icon_sprite_clut(page->icons.icons[0]);
        menu_select_icon_sprite_clut(page->icons.icons[1]);
        menu_select_icon_sprite_clut(page->icons.icons[2]);
        if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }
        menu_update_icon_cursor_sprites(entry, &page->icons.base, i, cursor);
        if (layout->columns.hidden_rows != 0 && thread_is_previous_running() == 0) {
            if (row_offset != 0) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->columns.hidden_rows) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0
                = top + span * entry->selected_index / (layout->columns.row_count + layout->columns.hidden_rows) - 2;
            gfx_draw_or_append_gpu_primitive(&page->thumb);
        }
        if (g_world_menu_scrolling_list_depth < 2 && delta == 0) {
            menu_handle_action(entry, cursor);
        }
        menu_handle_cancel_input(entry);
        if ((g_script_event_speed == 1 && !(script_get_event_initial_repeat_counter() & 1))
            || g_script_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step;
            } else {
                step = g_main_menu_scroll_fast_step;
            }
            accum += delta * (step << 1);
        }
        timer += g_script_event_speed;
        if (accum < -0xF || accum > 0xF) {
            ring += delta;
            row_offset += delta;
            repeat = script_get_event_initial_repeat_counter();
            accum = 0;
            delta = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_script_event_speed) {
                g_script_event_input |= g_script_event_current_input & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
            menu_free_memory(buffer);
            row_buffer_live = 0;
        }
        frame->v0 = (ring * 16 + accum - 2) & 0x7F;
        if (i * g_script_event_speed < 12) {
            gfx_init_image_loading(
                &page->frame, &entry->vram_origin, &entry->window, (const image_load_parameters_t*)&window);
            menu_zoom_cursor_frame(entry, frame, i * g_script_event_speed);
        }
        if (layout->columns.mode[1] == MENU_TEXT_COLUMN_TEXT && toggle == 1) {
            menu_text_layout_t* columns = entry->context;

            page->icons.base.sprites[1].x0 += columns->x[1];
            columns = entry->context;
            page->icons.base.sprites[2].x0 += columns->x[1];
        }
        page->icons.base.sprites[1].y0 += 4;
        page->icons.base.sprites[2].y0 += 4;
        if (entry->header_id >= 5) {
            gfx_draw_or_append_gpu_primitive(page->icons.icons[0]);
            gfx_draw_or_append_gpu_primitive(page->icons.icons[1]);
            if (g_thread_contexts[g_thread_current_id].function_parameter_4 != 0) {
                gfx_draw_or_append_gpu_primitive(page->icons.icons[2]);
            }
        }
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[3]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[1]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[2]);
        gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        gfx_draw_or_append_gpu_primitive(frame);
        gfx_draw_or_append_gpu_primitive(&page->text_mode);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.draw_mode);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[0]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base);
        i++;
    }
    if (row_buffer_live != 0) {
        menu_free_memory(buffer);
    }
    thread_yield();
    g_world_menu_scrolling_list_depth--;
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}
#endif

void text_set_message_duration_frames(u16 frames) {
    g_text_message_duration_frames = frames;
}

/* Size a message window to its text and center it on x = 0x102. */
static void menu_size_entry_to_text(menu_entry_t* entry, s16* width, s16* height, s32* out_pad, s32 pad_short) {
    const u8* text;
    u16 padded;
    u16 w;
    u16 h;

    text = text_find_entry(entry->text_id);
    *out_pad = 0;
    text_measure_pixels(width, height, text);
    if (pad_short != 0) {
        if (*width < 8 && *height == 1) {
            *out_pad = (8 - *width) * 5;
            *width = 8;
        }
    }
    padded = *width + 0x18;
    *width = padded + (padded & 3);
    *height = *height * 16 + 16;
    entry->window.x = 0x102 - (*width >> 1);
    w = *width;
    entry->text_image.width = w;
    entry->inner_width = w;
    entry->window.w = w;
    h = *height;
    entry->text_image.height = h;
    entry->inner_height = h;
    entry->window.h = h;
}

/* Timed menu icon thread: shows one text entry with alternating sprite
 * records until input arrives or the g_battle_text_message_duration_frames frame budget (scaled by the
 * event speed) runs out. Bit 8 of the budget suppresses cancel handling
 * until the budget expires. The thread-id check reads an unset local in the
 * original. */
void menu_icon_timed_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 hold;
    menu_icon_record_t* record;
    s16 width;
    s16 height;
    s32 pad;
    s32 unset;

    hold = 0;
    if (g_text_message_duration_frames >= 0x100) {
        hold = 1;
        g_text_message_duration_frames &= 0xFF;
    }
    i = 0;
    param = (menu_entry_t*)thread_get_current_parameter_1();
    menu_size_entry_to_text(param, &width, &height, &pad, 1);
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], 0x7C);
    buffer = menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = width;
    menu_set_text_origin(pad + 8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, (u32*)buffer);
    for (;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        if (g_text_message_duration_frames < i * g_script_event_speed) {
            hold = 0;
            g_script_event_input = PSX_PAD_CIRCLE;
            g_sound_suppressed = 1;
        }
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        if (unset >= 0) {
            g_thread_current_id--;
            menu_select_icon_cluts(&records[i & 1].base);
            g_thread_current_id++;
        } else {
            menu_select_icon_cluts(&records[i & 1].base);
        }
        record = &records[i & 1];
        menu_update_icon_cursor_sprites(param, &record->base, i, -1);
        if (hold == 0) {
            menu_handle_action(param, 0);
            menu_handle_cancel_input(param);
        }
        menu_submit_icon_primitives(&record->base);
    }
    g_menu_panel_fade_intensity = 0;
    g_sound_suppressed = 0;
    thread_yield();
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}

/* Menu icon thread that also opens a linked entry: when row_actions[0]
 * names a menu entry, that entry is centerd on this text width, placed below
 * this entry, and started in the previous thread slot. The icon records then
 * alternate until input completes, selecting CLUTs in the linked slot. */
void menu_icon_linked_entry_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 parent;
    menu_icon_record_t* record;
    s16 width;
    s16 height;
    s32 pad;

    param = (menu_entry_t*)thread_get_current_parameter_1();
    menu_size_entry_to_text(param, &width, &height, &pad, 0);
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], 0x7C);
    parent = param->row_actions[0];
    if (parent >= 0) {
        /* Unsigned narrowing keeps the retail 16-bit centering expression. */
        g_menu_thread_menu_data[parent].window.x
            = (u16)((width >> 1) - (g_menu_thread_menu_data[parent].inner_width - 0x102));
        g_menu_thread_menu_data[parent].window.y = param->window.y + param->window.h;
        thread_start(g_thread_current_id - 1, g_menu_thread_menu_data[parent].thread_entry);
        thread_set_parameters(g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[parent], 0, 0);
    }
    buffer = menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = width;
    menu_set_text_origin(8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, (u32*)buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        if (parent >= 0) {
            g_thread_current_id--;
            menu_select_icon_cluts(&records[i & 1].base);
            g_thread_current_id++;
        } else {
            menu_select_icon_cluts(&records[i & 1].base);
        }
        record = &records[i & 1];
        menu_update_icon_cursor_sprites(param, &record->base, i, -1);
        menu_handle_action(param, 0);
        menu_handle_cancel_input(param);
        menu_submit_icon_primitives(&record->base);
    }
    g_sound_suppressed = 0;
    thread_yield();
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}

void menu_input_thread(void) {
    menu_entry_t* menu;

    menu = (menu_entry_t*)thread_get_current_parameter_1();
    g_sound_suppressed = 1;
    while (1) {
        thread_yield();
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(menu, 0);
        menu_handle_cancel_input(menu);
    }
    thread_yield();
    g_sound_suppressed = 0;
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}

/* Read a script variable from any of the 32-bit variable, flag and nibble ranges by adding
 * it into 32-bit script variable 0 with the variable commands. Variable 0 (the comparison
 * result) is restored before returning.
 *
 * Reading the combined weather/time variable first rebuilds it: bit 15 from bit 0
 * of the time of day and bits 12-14 from the low three bits of the weather. GNS
 * mesh selection combines those upper bits with the layout's low 12 bits. */
s32 script_get_variable(s32 variable_id) {
    s32 saved_current_variable = *g_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WEATHER_AND_TIME) {
        s32 time_of_day;
        s32 weather;

        time_of_day = script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY);
        time_of_day &= 1;
        weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        weather &= 7;
        time_of_day <<= 15;
        weather <<= 12;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, time_of_day | weather);
    }
    script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, EVENT_SCRIPT_VAR_COMPARISON_RESULT, 0, 0);
    script_run_variable_command(EVENT_OPCODE_ADD_VARIABLE, EVENT_SCRIPT_VAR_COMPARISON_RESULT, variable_id, 0);
    {
        s32 value = *g_script_variables;

        *g_script_variables = saved_current_variable;
        return value;
    }
}

/* Store a value in a script variable, whichever of the 32-bit variable, flag and nibble
 * ranges it belongs to, by running the ZeroVariable and AddImmediate commands on
 * it. 32-bit script variable 0 (the comparison result) is preserved around them, and war
 * funds are capped at 99,999,999. */
void script_set_variable(s32 variable_id, s32 value) {
    s32 saved_current_variable = *g_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WAR_FUNDS && value > 0x05f5e0ff) {
        value = 0x05f5e0ff;
    }
    script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, variable_id, 0, 0);
    script_run_variable_command(EVENT_OPCODE_ADD_IMMEDIATE, variable_id, value, 0);
    *g_script_variables = saved_current_variable;
}

void menu_init_panel_fade_tiles(void) {
    menu_gradient_panel_prims_t* frame;
    s32 i;

    frame = &g_menu_panel_fade_frames[0];

    SetDrawMode(&frame->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);

    SetTile(&frame->center);
    setRGB0(&frame->center, 0, 0, 0);
    SetSemiTrans(&frame->center, 1);
    frame->center.x0 = 0x80;
    frame->center.y0 = 0xac;
    frame->center.w = 0x100;
    frame->center.h = 0x32;

    for (i = 0; i < 9; i++) {
        SetTile(&frame->top[i]);
        setRGB0(&frame->top[i], 0, 0, 0);
        SetSemiTrans(&frame->top[i], 1);
        frame->top[i].w = 0x100;
        frame->top[i].h = 1;
        frame->top[i].x0 = 0x80;
        frame->top[i].y0 = i + 0xa3;

        SetTile(&frame->bottom[i]);
        setRGB0(&frame->bottom[i], 0, 0, 0);
        SetSemiTrans(&frame->bottom[i], 1);
        frame->bottom[i].w = 0x100;
        frame->bottom[i].h = 1;
        frame->bottom[i].x0 = 0x80;
        frame->bottom[i].y0 = i + 0xde;
    }

    mem_copy_bytes_nonempty(
        &g_menu_panel_fade_frames[1], &g_menu_panel_fade_frames[0], sizeof(menu_gradient_panel_prims_t));
}

/* Update panel brightness and submit the center and edge tiles. */
void menu_update_panel_fade(void) {
    menu_gradient_panel_prims_t* frame;
    s32 i;
    s32 intensity;

    if (g_menu_panel_fade_mode != MENU_PANEL_FADE_NONE) {
        frame = &g_menu_panel_fade_frames[g_menu_packet_buffer_index];
        if (g_menu_panel_fade_mode == MENU_PANEL_FADE_IN && g_menu_panel_fade_intensity < 80) {
            g_menu_panel_fade_intensity += 8;
        }
        if (g_menu_panel_fade_mode == MENU_PANEL_FADE_OUT) {
            if (g_menu_panel_fade_intensity != 0) {
                g_menu_panel_fade_intensity -= 8;
            } else {
                g_menu_panel_fade_mode = MENU_PANEL_FADE_NONE;
            }
        }
        setRGB0(&frame->center, g_menu_panel_fade_intensity, g_menu_panel_fade_intensity, g_menu_panel_fade_intensity);
        intensity = g_menu_panel_fade_intensity;
        for (i = 0; i < 9; i++) {
            if (intensity != 0) {
                intensity -= 8;
            }
            /* The pointer walk preserves the target's operand order for the biased index. */
            (frame->top + (8 - i))->r0 = intensity;
            (frame->top + (8 - i))->g0 = intensity;
            (frame->top + (8 - i))->b0 = intensity;
            setRGB0(&frame->bottom[i], intensity, intensity, intensity);
            gfx_draw_or_append_gpu_primitive(&frame->top[8 - i]);
            gfx_draw_or_append_gpu_primitive(&frame->bottom[i]);
        }
        gfx_draw_or_append_gpu_primitive(&g_menu_panel_fade_frames[g_menu_packet_buffer_index].center);
        gfx_draw_or_append_gpu_primitive(&g_menu_panel_fade_frames[g_menu_packet_buffer_index].draw_mode);
    }
}

/* Draws a full-screen semi-transparent tile of gray level |level|: positive
 * values subtract (abr 2), negative values add (abr 1), and 0 draws nothing.
 *
 * The draw mode is submitted
 * before the tile while an ETC full-screen graphic draws primitives immediately,
 * and after it when both are linked into the ordering table. A non-zero level
 * also clears g_script_event_input, so menus see no button while the
 * screen is faded. */
void gfx_submit_screen_fade_overlay(fade_overlay_t* overlay, s32 level) {
    s32 blend;

    if (level != 0) {
        SetTile(&overlay->tile);
        SetSemiTrans(&overlay->tile, 1);
        blend = 2;
        overlay->tile.w = 256;
        overlay->tile.h = 240;
        setRGB0(&overlay->tile, 0, 0, 0);
        overlay->tile.x0 = 128;
        overlay->tile.y0 = 0;
        if (level < 0) {
            blend = 1;
            level = -level;
        }
        SetDrawMode(&overlay->mode, 0, 0, GetTPage(0, blend, 0x3c0, 0x100), &g_gfx_texture_window);
        setRGB0(&overlay->tile, level, level, level);
        if (g_gfx_draw_primitives_immediately != 0) {
            gfx_draw_or_append_gpu_primitive(&overlay->mode);
            gfx_draw_or_append_gpu_primitive(&overlay->tile);
        } else {
            gfx_draw_or_append_gpu_primitive(&overlay->tile);
            gfx_draw_or_append_gpu_primitive(&overlay->mode);
        }
        g_script_event_input = 0;
    }
}

static void menu_reserve_high_overlay(u32 size) {
    while (1) {
        if (main_heap_reserve_at(g_main_heap_high_overlay_load_address, size)
            == g_main_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_menu_wait_for_file_allocation_message);
        }
    }
}

static void menu_free_high_overlay(void) {
    main_heap_free(g_main_heap_high_overlay_load_address);
}

/* Publish an executable request, wait for it to clear, then honor the frame exit request. */
void menu_request_open_companion_executable(s32 request_index) {
    while (g_menu_companion_executable_request_index != 0 || g_menu_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_menu_companion_executable_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_menu_companion_executable_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_exit_current();
    }
}

/* Publish a data-file request, wait for it to clear, then honor the frame exit request. */
static void menu_request_open_companion_data_file(s32 request_index) {
    while (g_menu_companion_executable_request_index != 0 || g_menu_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_menu_companion_data_file_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_menu_companion_data_file_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_exit_current();
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_request_open_companion_data_file and
 * script_execute_display_conditions_instruction. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void script_noop_1(void) {
}

/* Thread body of DisplayConditions: draw numeric values again and run REQUIRE's
 * condition dispatcher, which takes the display id and its operand from the
 * thread parameters. */
void script_execute_display_conditions_instruction(void) {
    g_menu_hide_numeric_values = 0;
    require_condition_dispatch();
}

/* The thread EVENT_OPCODE_DARK_SCREEN starts: it takes the task id, and in
 * BATTLE loads REQUIRE.OUT and runs its bonus image burst (WORLD has no
 * REQUIRE.OUT to run). */
void script_dark_screen_thread(void) {
    thread_set_current_task_id(THREAD_TASK_DARK_SCREEN);
#ifdef FFT_BATTLE
    menu_request_open_companion_executable(COMPANION_OVERLAY_REQUIRE);
    require_reward_animate_bonus_image_burst();
#endif
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_dark_screen_thread and text_run_spell_quote_thread. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_2(void) {
}

/* Task 0x46: show message 0xa800 | parameter 2 about the unit whose battle id
 * is parameter 1.
 *
 * The id also fills the three text substitution values. The text thread runs
 * with window mode 0x11, or 0x12 when the unit is on the upper half of the
 * screen, from SPELL.MES (companion data file 4) loaded into a 16 KiB
 * buffer. WORLD has no unit messages: its copy ends at once. */
void text_run_spell_quote_thread(void) {
#ifdef FFT_BATTLE
    SVECTOR screen_coords;
    unit_stats_t* unit;
    s32 id; /* Battle id, then the unit's unit_id: one variable in the target. */
    s32 window_mode;
    s32 message_id;

    g_menu_system_function_thread_busy = 1;
    thread_set_current_task_id(THREAD_TASK_UNIT_MESSAGE);
    script_set_event_speed(2);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    thread_wait_frames(3);
    window_mode = 0x11;
    id = thread_get_current_parameter_1();
    g_text_substitution_values[0] = id;
    g_text_substitution_values[1] = id;
    g_text_substitution_values[2] = id;
    unit = battle_unit_get_stats_from_battle_id(id);
    unit_project_to_screen(battle_unit_get_runtime_id_by_battle_id(id), &screen_coords);
    id = unit->unit_id;
    if (screen_coords.vy < 0x80) {
        window_mode = 0x12;
    }
    message_id = thread_get_current_parameter_2() | 0xA800;
    g_menu_companion_data_file_buffer = main_heap_alloc(0x4000);
    menu_request_open_companion_data_file(BATTLE_COMPANION_FILE_SPELL_MES);
    g_text_section_pointers[TEXT_SECTION_UNIT_MESSAGE] = g_menu_companion_data_file_buffer;
    thread_start(THREAD_SLOT_MENU, text_run_dialog_thread_variant);
    thread_set_parameters(THREAD_SLOT_MENU, window_mode, message_id, id);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    main_heap_free(g_menu_companion_data_file_buffer);
    g_menu_pending_selection[0] = 7;
    script_set_event_speed(1);
    g_menu_system_function_thread_busy = 0;
    thread_exit_current();
#endif
}

void menu_open_option_game_options(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_GAME_OPTIONS);
}

/* Thread entry of menu entry 0x1d: load OPTION.OUT and open its AT list. */
void menu_open_option_at_list(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_AT_LIST);
}

/* Thread started for system function 0x1f (crystal or treasure): load OPTION.OUT
 * and open its dead-unit panel. */
void menu_open_option_dead_unit_panel(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_DEAD_UNIT);
}

/* Wait for the camera and menu transition, then run Triangle-menu selection. */
void menu_run_triangle_selection(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    while (battle_camera_is_active() != 0 || g_menu_transition_state != 0) {
        thread_yield();
    }
    script_pulse_tutorial_wait_value(0xFE);
    g_menu_hide_numeric_values = 0;
    menu_run_icon_selection_loop();
}

/* Run BUNIT, release its high-overlay memory, then restore the unit view. */
static void menu_run_bunit_return_thread(void) {
    s32 i;
    s32 unused[2]; /* keeps the original 0x28 frame */

    g_menu_transition_state = 4;
    do {
        thread_yield();
    } while (thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0);

    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_BUNIT;
    g_menu_help_open = 0;
    do {
        thread_wait_frames(1);
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);

    menu_free_high_overlay();
    for (i = 2; i < 15; i++) {
        thread_set_parameters(i, 0, 0, 1);
    }
    for (i = 0; i < 0x100; i += 8) {
        g_gfx_screen_fade = 0xFF - i;
        thread_yield();
    }
    menu_clear_modal_open();
    g_menu_overlay_state = 0;
    g_gfx_screen_fade = 0;
    g_menu_transition_state = 0;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_HOVERED_UNIT, g_menu_selected_unit.battle_id, 0xFF);
    thread_exit_current();
}

/* Fade out and load BUNIT, then start its return thread. */
static void menu_run_bunit_transition_thread(void) {
    s32 level;

    for (level = 0; level < 0x100; level += 0x10) {
        g_gfx_screen_fade = level;
        thread_yield();
    }
    g_gfx_screen_fade = 0xFF;
    g_menu_overlay_state = 1;
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_UNIT_SUMMARY_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_RIGHT_UNIT_STATS, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_HOVERED_UNIT_STATS, 0, 0, 1);
    thread_wait_for_unit_view_panels();
    menu_request_open_companion_executable(COMPANION_OVERLAY_BUNIT);
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_BUNIT_SECOND_HALF);
    thread_wait_frames(2);
    g_menu_help_open = 1;
    thread_start(THREAD_BORROWED_SLOT_FADE_OUT, menu_run_bunit_return_thread);
    thread_exit_current();
}

/* Start the unit-list transition in a child slot and wait for it to finish. */
void menu_open_bunit(void) {
    menu_set_modal_open();
    thread_start(g_thread_current_id - 1, menu_run_bunit_transition_thread);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_exit_current();
}

/* Reserve and load the help overlay, then run its battle-help screen. */
static void menu_run_help_menu_thread(void) {
    if (thread_find_running_by_task(THREAD_TASK_UNIT_EDITOR_PANEL) != 0
        && thread_find_running_by_task(THREAD_TASK_MENU_WINDOW_BUILD) == 0) {
        sound_queue_sfx_confirm();
    }
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_HELPMENU);
    menu_run_battle_help();
}

/* Run the job-status overlay while its state is nonzero. */
void menu_run_jobstts_thread(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        thread_exit_current();
    }
    if (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
        thread_exit_current();
    }
    menu_set_modal_open();
    sound_queue_sfx_confirm();
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_JOBSTTS);
    thread_wait_frames(2);
    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_JOBSTTS;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_script_event_input = 0;
    menu_free_high_overlay();
    thread_wait_frames(2);
    menu_clear_modal_open();
    thread_exit_current();
}

/* Run the EQUIP overlays, then restore the menu threads and icon-list cursors. */
void menu_run_equip_thread(void) {
    s32 saved;
    s32 i;
    s32 j;

    menu_set_modal_open();
    saved = g_menu_unit_view_mode;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    g_menu_overlay_state = 1;
    thread_wait_for_unit_view_panels();
    menu_request_open_companion_executable(COMPANION_OVERLAY_EQUIP);
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_EQUIP_SECOND_HALF);
    thread_suspend(THREAD_SLOT_MENU);
    thread_suspend(THREAD_SLOT_MENU_CHILD);
    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_EQUIP;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_menu_thread_menu_data = g_menu_idle_action_entries;
    g_menu_action_slot_row_actions[0] = -3;
    g_sound_suppressed = 1;
    g_script_event_input |= PSX_PAD_CIRCLE;
    if (g_menu_companion_overlay_primitives == 0) {
        thread_resume(THREAD_SLOT_MENU);
        thread_resume(THREAD_SLOT_MENU_CHILD);
    }
    for (i = 9; i < 15; i++) {
        thread_set_parameters(i, 0, 0, 1);
    }
    thread_wait_frames(4);
    if (g_menu_companion_overlay_primitives != 0) {
        for (j = 0; j < 2; j++) {
            g_menu_pending_selection[j]
                = ((menu_entry_t*)g_thread_contexts[8 - j].function_parameter_1)->selected_index;
        }
    }
    menu_free_high_overlay();
    g_menu_overlay_state = 0;
    menu_start_unit_view_thread(saved, 0xFF, 0xFF);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_clear_modal_open();
    thread_exit_current();
}

/* Open the unit editor for a rider or mount, then restore its menu view. */
void menu_open_debugchr_unit_editor(void) {
    s16* unit_index = &g_menu_selected_unit.battle_id;

    if (battle_unit_get_stats_from_battle_id(*unit_index)->mount_info != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
        menu_set_modal_open();
        g_menu_overlay_state = 1;
        menu_request_open_companion_executable(COMPANION_OVERLAY_DEBUGCHR);
        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
        thread_wait_until_inactive(THREAD_SLOT_MENU);
        thread_wait_for_unit_view_panels();
        menu_record_selection_for_selected_unit();
        battle_menu_enter_status_screen_selection();
        debugchr_editor_run_unit_thread(*unit_index);
        g_menu_overlay_state = 0;
        g_sound_suppressed = 0;
        g_script_event_input = 0;
        menu_build_idle_action_menu(g_menu_current_id);
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, *unit_index, *unit_index);
        menu_clear_modal_open();
        battle_menu_leave_status_screen_selection();
        thread_exit_current();
    }
}

/* Close the panels, run the requested debug session, then restore menu state. */
void menu_open_debugchr(void) {
    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    menu_set_modal_open();
    g_menu_overlay_state = 1;
    menu_request_open_companion_executable(COMPANION_OVERLAY_DEBUGCHR);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    thread_wait_for_unit_view_panels();
    thread_yield();
    thread_yield();
    debugchr_editor_run_session();
    g_menu_overlay_state = 0;
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_clear_modal_open();
    thread_exit_current();
}

void script_show_graphic(void) {
    thread_set_current_task_id(THREAD_TASK_SHOW_GRAPHIC);
    menu_request_open_companion_executable(COMPANION_OVERLAY_ETC);
    etc_graphic_show_async();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_show_graphic and
 * menu_load_portrait_image. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_3(void) {
}

/* Load portrait sector 0x164b + 4 * portrait_id straight into VRAM. */
void menu_load_portrait_image(s32 portrait_id) {
    menu_portrait_image_file_t* buffer;

    buffer = menu_alloc_memory(sizeof(*buffer));
    g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer);
    LoadImage(&g_menu_portrait_pixel_rect, buffer->pixels[0][0]);
    LoadImage(&g_menu_portrait_palette_rect, (u32*)buffer->palettes);
    menu_free_memory(buffer);
}

/*
 * Loads one 0x7800-byte block of EVTCHR.BIN into a fresh allocation on the main
 * stack, publishing it through g_battle_event_pending_loaded_evtchr_slot / g_battle_event_loaded_evtchr_buffer before
 * releasing it. Parameter 2 selects the block; each occupies 15 sectors from 0x1d4c.
 */
void gfx_load_evtchr_thread(void) {
    s32 parameter;
    s32 block;
    evtchr_file_record_t* buffer;

    parameter = thread_get_current_parameter_1();
    block = thread_get_current_parameter_2();
    thread_set_current_task_id(THREAD_TASK_LOAD_EVTCHR);
    buffer = main_heap_alloc(0x7800);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack((block * 15) + DISC_SECTOR_EVENT_EVTCHR_BIN, 0x7800, buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);
    g_script_pending_loaded_evtchr_slot = parameter;
    g_script_loaded_evtchr_buffer = buffer;
    thread_yield();
    main_heap_free(buffer);
    thread_exit_current();
}

/* Uploads eight 32x48 portraits and their palettes from one EVTFACE.BIN block. */
void script_load_portrait_colors_event_instruction(s32 portrait_id) {
    RECT rect;
    menu_portrait_image_file_t* buffer;
    u32(*pixels)[48][4];
    s32 i;
    palette_t* palette;

    buffer = menu_alloc_memory(sizeof(*buffer));
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);

    palette = buffer->palettes;
    mem_copy_bytes_nonempty(&rect, &g_menu_portrait_pixel_rect, sizeof(rect));
    i = 0;
    pixels = buffer->pixels;
    rect.w = 8;
    rect.h = 0x30;
    do {
        LoadImage(&rect, pixels[0][0]);
        rect.x += 8;
        pixels += 1;
        i += 1;
    } while (i < 8);
    LoadImage(&g_menu_portrait_palette_rect, (u32*)palette);
    menu_free_memory(buffer);
    thread_yield();
}

/* Read event block `id` (EVENT_BLOCK_SECTORS sectors from the event archive) into
 * the script event block, yielding until the load finishes, then turn the block's
 * text offset into the event-message section pointer.
 *
 * The 32-bit offset is then overwritten with EVENT_TEXT_OFFSET_PROCESSED: the
 * interpreter starts at byte zero of the block and reads those four bytes as
 * one-byte NOP opcodes. A block whose first 32-bit value already holds that marker leaves
 * the section pointer alone. Both the read request and the load poll run on the
 * main stack. */
void script_load_event(s32 id) {
    event_file_block_t* block;
    u32 off;

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack(
                 id * EVENT_BLOCK_SECTORS + DISC_SECTOR_EVENT_TEST_EVT, EVENT_BLOCK_BYTES, g_main_script_event_block)
        != 0);

    do {
        thread_yield();
        g_thread_call_target = main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);

    block = g_script_event_block;
    off = block->text_offset_or_marker;
    if (off != EVENT_TEXT_OFFSET_PROCESSED) {
        g_text_section_pointers[TEXT_SECTION_EVENT_MESSAGES] = (u8*)(off + (u32)block);
        block->text_offset_or_marker = EVENT_TEXT_OFFSET_PROCESSED;
    }
}

void menu_set_palette_colors(const menu_palette_pair_t* source) {
    rgba5551_t* destination;

    destination = g_menu_palette_colors.colors;
    mem_copy_bytes_nonempty(destination, source->colors, sizeof(palette_t));
    mem_copy_bytes_nonempty(destination + PALETTE_COLOR_COUNT, source->colors + PALETTE_COLOR_COUNT, sizeof(palette_t));
}

void menu_init_palette_and_text(s32 unused_0) {
    menu_set_palette_colors(&g_menu_default_palette_colors);
    text_init_section_pointers();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_init_palette_and_text and menu_open_companion_data_file. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void menu_noop_4(void) {
}

/* Non-blocking two-step file load driven by g_thread_call_target:
 * issue the read for the pending request, then poll until it completes and
 * clear the request. */
void menu_open_companion_data_file(void) {
    if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading == 0)) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(g_menu_companion_data_file_lbas[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_sizes[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_buffer)
            == 0) {
            g_menu_companion_data_file_loading = 1;
        }
    } else if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading != 0)) {
        g_thread_call_target = main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_menu_companion_data_file_loading = 0;
            g_menu_companion_data_file_request_index = 0;
        }
    }
}

/* Per-frame driver of menu_request_open_companion_executable: start the
 * disc read of the requested overlay on the main stack, at its offset from the
 * companion overlay load address, then poll until the read finishes and clear the
 * request. */
void menu_open_companion_executable(void) {
    s32 idx;

    idx = g_menu_companion_executable_request_index;
    if ((idx != 0) && (g_menu_companion_executable_loading == 0)) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(g_menu_companion_executable_lbas[idx], g_menu_companion_executable_sizes[idx],
                g_menu_companion_executable_offsets[idx] + (s32)g_main_heap_companion_overlay_load_address)
            == 0) {
            g_menu_companion_executable_loading = 1;
        }
    } else if ((g_menu_companion_executable_request_index != 0) && (g_menu_companion_executable_loading != 0)) {
        g_thread_call_target = main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_menu_companion_executable_loading = 0;
            g_menu_companion_executable_request_index = 0;
        }
    }
}

void menu_reset_unit_records_wrapper(void) {
    menu_reset_unit_records();
}

/* Empty: the overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void menu_run_companion_overlay_stub(void) {
}

static void menu_start_entry_thread(void) {
    s32 menu_id = thread_get_current_parameter_1();
    s32 count;

    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_overlay_state != 0) {
        thread_exit_current();
    }
    camera_wait_until_idle();
    while (thread_find_running_by_task(THREAD_TASK_ATTACK_TARGET_STATS) != 0) {
        thread_yield();
    }
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        if (script_is_tutorial_event_slot() != 0) {
            g_menu_input_mode = MENU_INPUT_TUTORIAL;
            script_set_variable(
                EVENT_SCRIPT_VAR_CURRENT_EVENT, script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) + 1);
            script_start_current_event_thread();
            thread_wait_frames(0x1E);
        }
        if (g_menu_input_mode != MENU_INPUT_PLAYER) {
            thread_wait_frames(2);
        }
    } else {
        thread_wait_frames(2);
    }
    script_pulse_tutorial_wait_value(0xFF);
    if (menu_id == BATTLE_MENU_ID_OTHER_UNIT || menu_id == BATTLE_MENU_ID_OTHER_UNIT_INSPECT) {
        if (menu_get_selected_unit_stats()->unit_flags & UNIT_FLAG_EGG) {
            menu_id = BATTLE_MENU_ID_EGG;
        }
    }
    thread_start(THREAD_SLOT_MENU, g_menu_thread_menu_data[menu_id].thread_entry);
    thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[menu_id], 0, 0);
    count = g_menu_restore_depth;
    g_script_event_input = 0;
    if (g_menu_restore_pending != 0 && g_menu_restore_unit_id == g_menu_selected_unit.battle_id) {
        g_menu_help_open = 1;
        thread_wait_frames(2);
        if (count > 0) {
            thread_start(THREAD_SLOT_MENU_CHILD, menu_run_skillset_thread);
            thread_set_parameters(
                THREAD_SLOT_MENU_CHILD, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST], 0, 0);
        }
        thread_wait_frames(2);
        if (count >= 2) {
            thread_start(THREAD_SLOT_MENU_GRANDCHILD, menu_select_unit_action_slots_thread);
            thread_set_parameters(
                THREAD_SLOT_MENU_GRANDCHILD, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST], 0, 0);
        }
        thread_wait_frames(2);
        g_menu_restore_pending = 0;
        g_menu_help_open = 0;
    } else {
        g_menu_restore_depth = 0;
        g_menu_restore_unit_id = g_menu_selected_unit.battle_id;
    }
    thread_exit_current();
}

enum {
    IDLE_ACTION_MENU_ENTRY_COUNT = 0x34,
};

/* Open an idle action menu (battle_menu_id_e) for the active-turn unit: place the
 * entry windows beside the unit, restore the row the unit last chose and start
 * the menu-building thread on menu_start_entry_thread.
 *
 * Nothing happens while menu input is blocked. The menu is also skipped while the
 * menus are suspended, a help window is open,
 * g_battle_menu_building_thread_started is set (it is cleared here either way) or
 * menu_id is BATTLE_MENU_ID_NONE.
 *
 * g_battle_menu_idle_action_entry_labels holds two window x bytes per entry, each
 * 0x80 less than the screen x; the second is used when the unit's projected x is
 * 0x101 or more, and a zero byte leaves the entry's x unchanged. WORLD only
 * clears the menu-building-started flag and does not read the menu id. */
void menu_build_idle_action_menu(s32 menu_id) {
#ifdef FFT_BATTLE
    SVECTOR screen_position;
    s32 i;
    s32 column;
    u8* label_table;
    menu_entry_t* entry;
    s16* selected_index;

    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        if ((g_menu_overlay_state == 0) && (g_menu_help_open == 0) && (g_menu_building_thread_started == 0)
            && (menu_id != BATTLE_MENU_ID_NONE)) {
            g_menu_current_id = menu_id;
            g_menu_thread_menu_data = g_menu_idle_action_entries;
            unit_project_to_screen(
                battle_unit_get_runtime_id_by_battle_id(g_menu_selected_unit.battle_id), &screen_position);
            i = 0;
            entry = g_menu_thread_menu_data;
            label_table = g_battle_menu_idle_action_entry_labels;
            /* Two window x bytes per entry; a projected x of 0x101 or more uses the second. */
            column = screen_position.vx >= 0x101;
            do {
                /* The offset-plus-base integer sum is byte-required (addu operand order). */
                u8* label = (u8*)(column + (u32)label_table);

                column += 2;
                if (*label != 0) {
                    entry->window.x = *label + 0x80;
                }
                i += 1;
                entry += 1;
            } while (i < IDLE_ACTION_MENU_ENTRY_COUNT);
            menu_restore_selection_for_selected_unit(menu_id);
            selected_index = &g_menu_thread_menu_data[8].selected_index;
            *selected_index = 0;
            g_menu_status_list_layout.row_offset = 0;
            menu_clear_entry_selections();
            thread_start(THREAD_SLOT_MENU_BUILDING, menu_start_entry_thread);
            thread_set_parameters(THREAD_SLOT_MENU_BUILDING, menu_id, 0, 0);
        }
        g_menu_building_thread_started = 0;
    }
#else
    g_menu_building_thread_started = 0;
#endif
}

void menu_start_building_thread(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU) != 0 || thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        if (g_menu_current_id != BATTLE_MENU_ID_MINI_MENU) {
            g_menu_building_thread_started = 1;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        return;
    }
    if (g_menu_modal_open != 0) {
        return;
    }
    if (battle_camera_is_active() != 0) {
        return;
    }
    if (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE) {
        return;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return;
    }
    g_menu_help_open = 1;
    thread_start(THREAD_SLOT_MENU_BUILDING, menu_run_help_menu_thread);
    thread_set_parameters(THREAD_SLOT_MENU_BUILDING, 0, 0, 0);
}

void menu_request_open_small(void) {
    g_menu_companion_executable_request_index = COMPANION_OVERLAY_SMALL;
}

/* Drive a pending companion overlay load during map initialization. Returns 1
 * while the request is still pending; once it completes the SMALL.OUT text
 * pointers are bound and it returns 0. With no request it returns 0 at once. */
s32 menu_poll_small_load(void) {
    if (g_menu_companion_executable_request_index == 0) {
        return 0;
    }
    menu_open_companion_executable();
    if (g_menu_companion_executable_request_index != 0) {
        return 1;
    }
    small_text_init_battle_pointers();
    return 0;
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * menu_poll_small_load and map_data_load_complete_thread. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
s32 map_return_zero_1(void) {
    return 0;
}

/* Mark map-data loading complete after one yield, then exit after another. */
void map_data_load_complete_thread(void) {
    thread_yield();
    battle_map_mark_data_load_complete();
    thread_yield();
    thread_exit_current();
}
