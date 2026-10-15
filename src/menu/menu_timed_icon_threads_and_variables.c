/* Message duration, window sizing, timed/linked icon threads, input thread and script variable get/set. */

#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void text_set_message_duration_frames(u16 frames) {
    g_text_message_duration_frames = frames;
}

/* Size a message window to its text and center it on x = 0x102. */
void menu_size_entry_to_text(menu_entry_t* entry, s16* width, s16* height, s32* out_pad, s32 pad_short) {
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
    param = (menu_entry_t*)thread_get_current_parameter_1_variant();
    menu_size_entry_to_text(param, &width, &height, &pad, 1);
    menu_build_window_sprites_variant(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], 0x7C);
    buffer = menu_build_and_upload_window_frame_image_variant(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
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
        if (menu_should_close_thread_variant(&g_script_event_input) != 0) {
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
    if (thread_get_current_parameter_2_variant() == 0) {
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

    param = (menu_entry_t*)thread_get_current_parameter_1_variant();
    menu_size_entry_to_text(param, &width, &height, &pad, 0);
    menu_build_window_sprites_variant(&rect, param, &records[0]);
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
    buffer = menu_build_and_upload_window_frame_image_variant(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = width;
    menu_set_text_origin(8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, (u32*)buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        if (menu_should_close_thread_variant(&g_script_event_input) != 0) {
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
    if (thread_get_current_parameter_2_variant() == 0) {
        thread_exit_current();
    }
}

void menu_input_thread(void) {
    menu_entry_t* menu;

    menu = (menu_entry_t*)thread_get_current_parameter_1_variant();
    g_sound_suppressed = 1;
    while (1) {
        thread_yield();
        if (menu_should_close_thread_variant(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(menu, 0);
        menu_handle_cancel_input(menu);
    }
    thread_yield();
    g_sound_suppressed = 0;
    if (thread_get_current_parameter_2_variant() == 0) {
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
