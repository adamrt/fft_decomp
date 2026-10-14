#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Scrolls a three-column text page by a row window on L1/R1 (or on the
 * running thread's redraw request), redraws the page into g_world_menu_text_page_image and
 * uploads it. Returns the image buffer when the page was redrawn. */
u8* world_menu_redraw_text_page_on_scroll(menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused_cursor) {
    menu_text_layout_t* layout = entry->context;
    u32* input;
    u32* input2;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;

    input = script_get_controller_input_pointer(0);
    input2 = script_get_controller_input_pointer(1);
    rect.x = entry->vram_origin.x;
    rect.y = entry->vram_origin.y;
    rect.w = entry->inner_width >> 2;
    rect.h = entry->inner_height;
    changed = 0;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        return 0;
    }
    if (!(*input2 & PSX_PAD_SQUARE) && g_threads[g_thread_current_id].task_words[4] == 0) {
        return 0;
    }
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
    if (changed == 0) {
        return 0;
    }
    menu_clear_render_buffer(g_world_menu_text_page_image, entry->inner_width * entry->inner_height / 2);
    menu_draw_text_columns(entry, row_offset, g_world_menu_text_page_image);
    *redraw = 0;
    LoadImage(&rect, g_world_menu_text_page_image);
    mem_copy_bytes_nonempty(&clear, &rect, sizeof(clear));
    clear.h = 2;
    clear.x = rect.x;
    clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
    clear.w = rect.w;
    ClearImage(&clear, 0, 0, 0);
    return g_world_menu_text_page_image;
}
