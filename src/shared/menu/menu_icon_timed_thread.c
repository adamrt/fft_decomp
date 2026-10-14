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
    param = (menu_entry_t*)battle_thread_get_current_parameter_1();
    menu_size_entry_to_text(param, &width, &height, &pad, 1);
    battle_menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], 0x7C);
    buffer = battle_menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
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
    g_battle_menu_panel_fade_intensity = 0;
    g_sound_suppressed = 0;
    thread_yield();
    if (battle_thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}
