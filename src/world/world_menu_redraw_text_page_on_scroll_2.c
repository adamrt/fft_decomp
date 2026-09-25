#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Scrolls a three-column text page by a row window on L1/R1 (or on the
 * running thread's redraw request), redraws the page into a freshly
 * allocated image buffer and uploads it. Returns that buffer when the page
 * was redrawn; the caller frees it. */
u8* world_menu_redraw_text_page_on_scroll_2(world_menu_entry_t* entry, s32* row_offset, s32* redraw) {
    world_menu_text_layout_t* layout = (world_menu_text_layout_t*)entry->text_binding;
    u32* input0;
    u32* input1;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    u8* buffer;

    input0 = world_input_get_menu_controller(0);
    input1 = world_input_get_menu_controller(1);
    rect.x = entry->vram_x;
    rect.y = entry->vram_y;
    rect.w = entry->inner_width >> 2;
    rect.h = entry->inner_height;
    changed = 0;
    if (!(*input1 & PSX_PAD_SQUARE) && g_world_threads[g_world_thread_current_id].task_words[4] == 0) {
        return 0;
    }
    if ((*input0 & PSX_PAD_UP) && *row_offset != 0) {
        offset = *row_offset - layout->row_count;
        *row_offset = offset;
        if (offset < 0) {
            *row_offset = 0;
        }
        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
        changed = 1;
    } else if ((*input0 & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
        offset = layout->row_count + *row_offset;
        *row_offset = offset;
        if (layout->hidden_rows < offset) {
            *row_offset = layout->hidden_rows;
        }
        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
        changed = 1;
    } else if (g_world_threads[g_world_thread_current_id].task_words[4] != 0) {
        changed = 1;
        g_world_threads[g_world_thread_current_id].task_words[4] = 0;
    }
    if (changed == 0) {
        return 0;
    }
    buffer = world_menu_alloc_ui_buffer(entry->inner_width * entry->inner_height / 2);
    world_clear_menu_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    world_menu_draw_text_columns(entry, row_offset, buffer);
    *redraw = 0;
    LoadImage(&rect, buffer);
    world_script_copy_bytes(&clear, &rect, 8);
    clear.h = 2;
    clear.x = rect.x;
    clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
    clear.w = rect.w;
    ClearImage(&clear, 0, 0, 0);
    return buffer;
}
