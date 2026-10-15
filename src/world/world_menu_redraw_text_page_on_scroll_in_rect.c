#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Sibling of world_screen_menu_redraw_text_page_on_scroll that draws through
 * world_draw_menu_text_columns_narrow and takes the image rectangle from a caller-supplied
 * RECT (x and w in framebuffer pixels, divided by four; the image row is
 * fixed at y = 0x80). Scrolls the row window on L1/R1 (or on the running
 * thread's redraw request), redraws the page into a freshly allocated image
 * buffer and uploads it. Returns that buffer when the page was redrawn. */
u8* world_menu_redraw_text_page_on_scroll_in_rect(
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
    menu_draw_text_columns_narrow_variant(entry, row_offset, buffer);
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
