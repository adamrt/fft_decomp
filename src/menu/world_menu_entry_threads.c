/* Entry value announce and clamped icon list threads. */

#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Menu thread: resolves the current entry's display value (kind 0x11 reads
 * the text binding's 16-bit table, masked to 11 bits, with 0x2b remapped
 * to 0x63 and bit 15 set; other kinds index g_world_menu_entry_text_id_tables) and hands it to the
 * text-character handler before parking the menu input state. */

void world_menu_announce_entry_value_thread(void) {
    menu_entry_t* entry;
    thread_t* thread;
    s32 kind;
    s32 index;
    s32 value;
    s32 next;

    entry = (menu_entry_t*)thread_get_current_parameter_1_variant();
    kind = entry->select_text_table;
    if (kind == 2 || kind == -1) {
        next = g_thread_current_id + 1;
        thread = &g_threads[next];
        while (1) {
            if (thread->is_running != 0) {
                entry = (menu_entry_t*)thread->function_parameter_1;
                if (entry->select_text_table != -1) {
                    break;
                }
            }
            thread++;
        }
        kind = entry->select_text_table;
    }
    index = entry->selected_index;
    if (kind == 0x11) {
        menu_text_binding_t* binding = entry->context;

        value = binding->row_text_ids[index];
        value &= 0x7FF;
        if (value == 0x2B) {
            value = 0x63;
        }
        value |= 0x8000;
    } else {
        value = g_world_menu_entry_text_id_tables[kind][index];
    }
    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
    thread_set_parameters(g_thread_current_id, 0x3F, value, 0);
    text_run_dialog_thread_variant();
    g_world_formation_menu_new_buttons = 0;
    g_menu_input_lock_mode = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu icon list thread with a clamped cursor: builds the text image once,
 * steps the cursor on the up/down buttons while the previous thread is idle
 * and submits the four icon sprites of the alternating record each frame. */
void world_menu_icon_clamped_list_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 cursor;
    menu_icon_record_t* record;
    menu_icon_sprites_t* sprites;

    g_world_menu_list_controller_input = script_get_controller_input_pointer(0);
    param = (menu_entry_t*)thread_get_current_parameter_1_variant();
    cursor = param->selected_index;
    record = &records[0];
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
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        record = &records[i & 1];
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        param->selected_index = cursor;
        if (menu_should_close_thread_variant(g_world_menu_list_controller_input) != 0) {
            break;
        }
        if (thread_is_previous_running_variant() == 0) {
            if ((*g_world_menu_list_controller_input & PSX_PAD_UP) && cursor != 0) {
                cursor--;
            }
            if (!(*g_world_menu_list_controller_input & PSX_PAD_DOWN) && cursor != param->max_row_index) {
                cursor++;
            }
            menu_handle_action(param, cursor);
            menu_handle_cancel_input(param);
        }
        sprites = &record->base;
        menu_select_icon_cluts(sprites);
        menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[3].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[2].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[1].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->draw_mode);
    }
    thread_yield();
    thread_exit_current();
}
