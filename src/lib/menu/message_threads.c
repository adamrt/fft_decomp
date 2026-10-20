/* Timed and linked text-message menus and their input thread. */

#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u16 g_message_duration_frames;

/* Set the frame budget of the next timed message (menu_icon_timed_thread); adding 0x100 also holds
 * off cancel until it runs out. */
void message_set_duration(u16 frames) {
    g_message_duration_frames = frames;
}

/* Size a message entry's window to its text and center it on x = 0x102.
 *
 * Returns the window size in *width and *height: the text's pixel width plus 24 for the
 * frame (then evened by adding its low two bits back), and 16 pixels per line plus 16. The
 * same size goes to the window, the text image and the inner area. With pad_short, a
 * one-line text under 8 pixels wide is widened to 8 and *out_pad receives a left indent of
 * 5 per missing pixel; otherwise *out_pad is 0. */
static void menu_size_entry_to_text(menu_entry_t* entry, s16* width, s16* height, s32* out_pad, s32 pad_short) {
    const u8* text = text_entry_find(entry->text_id);
    u16 padded;
    u16 w;
    u16 h;

    *out_pad = 0;
    text_pixels_measure(width, height, text);
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
    entry->text_image.source.w = w;
    entry->inner_width = w;
    entry->window.w = w;
    h = *height;
    entry->text_image.source.h = h;
    entry->inner_height = h;
    entry->window.h = h;
}

/* Timed menu icon thread: shows one text entry with alternating sprite
 * records until input arrives or the g_message_duration_frames frame budget (scaled by the
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
    if (g_message_duration_frames >= 0x100) {
        hold = 1;
        g_message_duration_frames &= 0xFF;
    }
    i = 0;
    param = (menu_entry_t*)thread_current_parameter_1_get();
    menu_size_entry_to_text(param, &width, &height, &pad, 1);
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(menu_icon_record_t));
    buffer = menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_window_text_pen.stride = width;
    menu_set_text_origin(pad + 8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_window_text_pen);
    LoadImage(&rect, (u32*)buffer);
    for (;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        /* Out of time: press Circle on the player's behalf, silently, so the menu closes
         * as if confirmed. */
        if (g_message_duration_frames < i * g_script_event_speed) {
            hold = 0;
            g_script_event_input = PSX_PAD_CIRCLE;
            g_sound_suppressed = 1;
        }
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        /* Retail bug: unset is never assigned. The linked-entry thread tests its linked
         * entry index here, selecting CLUTs in that entry's slot. */
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
    if (thread_current_parameter_2_get() == 0) {
        thread_current_exit();
    }
}

/* Menu icon thread that also opens a linked entry: when row_actions[0]
 * names a menu entry, that entry is centered on this text width, placed below
 * this entry, and started in the previous thread slot. The icon records then
 * alternate until input completes, selecting CLUTs in the linked slot. */
void menu_icon_linked_entry_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 linked_index;
    menu_icon_record_t* record;
    s16 width;
    s16 height;
    s32 pad;

    param = (menu_entry_t*)thread_current_parameter_1_get();
    menu_size_entry_to_text(param, &width, &height, &pad, 0);
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(menu_icon_record_t));
    linked_index = param->row_actions[0];
    if (linked_index >= 0) {
        /* Unsigned narrowing keeps the retail 16-bit centering expression. */
        g_menu_thread_menu_data[linked_index].window.x
            = (u16)((width >> 1) - (g_menu_thread_menu_data[linked_index].inner_width - 0x102));
        g_menu_thread_menu_data[linked_index].window.y = param->window.y + param->window.h;
        thread_start(g_thread_current_id - 1, g_menu_thread_menu_data[linked_index].thread_entry);
        thread_parameters_set(g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[linked_index], 0, 0);
    }
    buffer = menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_window_text_pen.stride = width;
    menu_set_text_origin(8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_window_text_pen);
    LoadImage(&rect, (u32*)buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        if (linked_index >= 0) {
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
    if (thread_current_parameter_2_get() == 0) {
        thread_current_exit();
    }
}

/* Menu thread with nothing to draw: with menu sounds muted, handle Circle on row 0 and cancel each
 * frame until the menu should close. */
void menu_input_thread(void) {
    menu_entry_t* menu = (menu_entry_t*)thread_current_parameter_1_get();

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
    if (thread_current_parameter_2_get() == 0) {
        thread_current_exit();
    }
}
