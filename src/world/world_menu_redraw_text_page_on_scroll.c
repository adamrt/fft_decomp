#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Scrolls a three-column text page by a row window on L1/R1 (or on the
 * running thread's redraw request), redraws the page into g_world_menu_text_page_image and
 * uploads it. Returns the image buffer when the page was redrawn. */
u8* world_menu_redraw_text_page_on_scroll(world_menu_entry_t* entry, s32* row_offset, s32* redraw) {
    world_menu_text_layout_t* layout = (world_menu_text_layout_t*)entry->text_binding;
    u32* input;
    u32* input2;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;

    input = world_input_get_menu_controller(0);
    input2 = world_input_get_menu_controller(1);
    rect.x = entry->vram_x;
    rect.y = entry->vram_y;
    rect.w = entry->inner_width >> 2;
    rect.h = entry->inner_height;
    changed = 0;
    if (g_world_menu_input_disabled != 0) {
        return 0;
    }
    if (!(*input2 & PSX_PAD_SQUARE) && g_world_threads[g_world_thread_current_id].task_words[4] == 0) {
        return 0;
    }
    if ((*input & PSX_PAD_UP) && *row_offset != 0) {
        offset = *row_offset - layout->row_count;
        *row_offset = offset;
        if (offset < 0) {
            *row_offset = 0;
        }
        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
        changed = 1;
    } else if ((*input & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
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
    world_clear_menu_render_buffer(g_world_menu_text_page_image, entry->inner_width * entry->inner_height / 2);
    world_menu_draw_text_columns_2(entry, row_offset, g_world_menu_text_page_image);
    *redraw = 0;
    LoadImage(&rect, g_world_menu_text_page_image);
    world_script_copy_bytes(&clear, &rect, 8);
    clear.h = 2;
    clear.x = rect.x;
    clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
    clear.w = rect.w;
    ClearImage(&clear, 0, 0, 0);
    return g_world_menu_text_page_image;
}
