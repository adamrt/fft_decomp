#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Sibling of world_menu_redraw_text_page_on_scroll_2 that draws through
 * world_draw_menu_text_columns_narrow and takes the image rectangle from a caller-supplied
 * RECT (x and w in framebuffer pixels, divided by four; the image row is
 * fixed at y = 0x80). Scrolls the row window on L1/R1 (or on the running
 * thread's redraw request), redraws the page into a freshly allocated image
 * buffer and uploads it. Returns that buffer when the page was redrawn. */
u8* world_menu_redraw_text_page_on_scroll_in_rect(
    world_menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area) {
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
    rect.x = (area->x / 4) + entry->vram_x;
    rect.y = 0x80;
    rect.w = area->w / 4;
    rect.h = area->h;
    changed = 0;
    if (g_world_menu_input_disabled != 0) {
        return 0;
    }
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
    world_draw_menu_text_columns_narrow(entry, row_offset, buffer);
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
