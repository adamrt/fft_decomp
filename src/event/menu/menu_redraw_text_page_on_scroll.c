/* Paging a text window by input, redrawing its columns and uploading the image. */

#include "fft/gfx.h"
#include "fft/main_sound.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Page a text window on Square+Up/Down (or on the running thread's redraw request): redraw its
 * three columns into g_menu_render_buffer and upload the image at the entry's VRAM origin.
 * Returns the buffer when the page was redrawn, else 0. */
u8* menu_redraw_text_page_on_scroll(menu_entry_t* menu, s32* first_row, s32* render_pending, s32* unused_cursor) {
    RECT source;
    RECT clear_strip;
    menu_text_layout_t* layout = menu->context;
    u32* pressed = script_get_controller_input_pointer(0);
    u32* held = script_get_controller_input_pointer(1);
    s32 changed;

    source.x = menu->vram_origin.x;
    source.y = menu->vram_origin.y;
    source.w = menu->inner_width >> 2;
    source.h = menu->inner_height;
    changed = 0;

    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        return 0;
    }

    if (((*held & PSX_PAD_SQUARE) == 0)
        && (g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] == 0)) {
        return 0;
    }

    if ((*pressed & PSX_PAD_UP) != 0 && *first_row != 0) {
        *first_row -= layout->row_count;
        if (*first_row < 0) {
            *first_row = 0;
        }
        changed = 1;
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
    } else if ((*pressed & PSX_PAD_DOWN) != 0 && *first_row != layout->hidden_rows) {
        *first_row += layout->row_count;
        if (*first_row > layout->hidden_rows) {
            *first_row = layout->hidden_rows;
        }
        changed = 1;
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
    } else if (g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] != 0) {
        changed = 1;
        g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] = 0;
    }

    if (changed == 0) {
        return 0;
    }

    menu_clear_render_buffer(g_menu_render_buffer, (menu->inner_width * menu->inner_height) / 2);
    menu_draw_text_columns(menu, first_row, g_menu_render_buffer);
    *render_pending = 0;
    LoadImage(&source, (u32*)g_menu_render_buffer);
    /* Clear the two VRAM rows above the image, wrapping within its 256-row page. */
    mem_copy_bytes_nonempty(&clear_strip, &source, sizeof(RECT));
    clear_strip.h = 2;
    clear_strip.x = source.x;
    clear_strip.y = ((source.y - 2) & 0xff) + (source.y & 0xff00);
    clear_strip.w = source.w;
    ClearImage(&clear_strip, 0, 0, 0);
    return g_menu_render_buffer;
}
