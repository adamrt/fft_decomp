#include "fft/battle_menu.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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

    param = (menu_entry_t*)battle_thread_get_current_parameter_1();
    menu_size_entry_to_text(param, &width, &height, &pad, 0);
    battle_menu_build_window_sprites(&rect, param, &records[0]);
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
    buffer = battle_menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
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
    if (battle_thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}
