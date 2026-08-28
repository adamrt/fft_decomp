/*
 * WORLD twin:
 * world_menu_redraw_text_page_on_scroll_in_rect, plus the mode-2 buffer
 * placement after the event overlay.
 */
#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Scrolls the row window of a three-column menu page on Square+Up/Down (or
 * on the running thread's redraw request), redraws the page into an image
 * buffer and uploads it to the caller-supplied rectangle (x and w in
 * framebuffer pixels, divided by four; the image row is fixed at y = 0x80).
 * In mode 2 of g_battle_menu_scroll_list_depth the buffer is placed 0x2000 bytes past the event
 * overlay instead of being allocated. Returns that buffer when the page was
 * redrawn. */
u8* battle_menu_redraw_text_page_on_scroll_in_rect(
    world_menu_entry_t* entry, s32* row_offset, s32* redraw, s32 unused, RECT* area) {
    world_menu_text_layout_t* layout = (world_menu_text_layout_t*)entry->text_binding;
    u32* input;
    u32* input2;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    u8* buffer;

    input = battle_script_get_controller_input_pointer(0);
    input2 = battle_script_get_controller_input_pointer(1);
    rect.x = (area->x / 4) + entry->vram_x;
    rect.y = 0x80;
    rect.w = area->w / 4;
    rect.h = area->h;
    changed = 0;
    if (g_battle_menu_input_disabled == 0
        && ((*input2 & PSX_PAD_SQUARE) || g_battle_threads[g_battle_current_thread_id].task_words[4] != 0)) {
        if ((*input & PSX_PAD_UP) && *row_offset != 0) {
            offset = *row_offset - layout->row_count;
            *row_offset = offset;
            if (offset < 0) {
                *row_offset = 0;
            }
            g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            changed = 1;
        } else if ((*input & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
            offset = layout->row_count + *row_offset;
            *row_offset = offset;
            if (layout->hidden_rows < offset) {
                *row_offset = layout->hidden_rows;
            }
            g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            changed = 1;
        } else if (g_battle_threads[g_battle_current_thread_id].task_words[4] != 0) {
            changed = 1;
            g_battle_threads[g_battle_current_thread_id].task_words[4] = 0;
        }
        if (changed != 0) {
            if (g_battle_menu_scroll_list_depth == 2) {
                buffer = (u8*)(g_event_overlay_load_address + 0x2000);
            } else {
                buffer = battle_menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
            }
            battle_clear_menu_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
            battle_menu_draw_text_columns_narrow(entry, row_offset, buffer);
            *redraw = 0;
            LoadImage(&rect, buffer);
            battle_copy_bytes(&clear, &rect, 8);
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
