#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Apply scroll input, rebuild the menu image, and upload the changed region. */
u8* menu_redraw_text_page_on_scroll(menu_entry_t* menu, s32* first_row, s32* render_pending, s32* unused_cursor) {
    RECT source;
    RECT clear_strip;
    menu_text_layout_t* layout;
    u32* primary_input;
    u32* secondary_input;
    s32 changed;

    layout = menu->context;
    primary_input = script_get_controller_input_pointer(0);
    secondary_input = script_get_controller_input_pointer(1);
    source.x = menu->vram_origin.x;
    source.y = menu->vram_origin.y;
    source.w = menu->inner_width >> 2;
    source.h = menu->inner_height;
    changed = 0;

    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        return 0;
    }

    if (((*secondary_input & PSX_PAD_SQUARE) == 0) && (g_threads[g_thread_current_id].task_words[4] == 0)) {
        return 0;
    }

    if ((*primary_input & PSX_PAD_UP) != 0 && *first_row != 0) {
        *first_row -= layout->row_count;
        if (*first_row < 0) {
            *first_row = 0;
        }
        changed = 1;
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
    } else if ((*primary_input & PSX_PAD_DOWN) != 0 && *first_row != layout->hidden_rows) {
        *first_row += layout->row_count;
        if (*first_row > layout->hidden_rows) {
            *first_row = layout->hidden_rows;
        }
        changed = 1;
        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
    } else if (g_threads[g_thread_current_id].task_words[4] != 0) {
        changed = 1;
        g_threads[g_thread_current_id].task_words[4] = 0;
    }

    if (changed == 0) {
        return 0;
    }

    menu_clear_render_buffer(g_menu_render_buffer, (menu->inner_width * menu->inner_height) / 2);
    menu_draw_text_columns(menu, first_row, g_menu_render_buffer);
    *render_pending = 0;
    LoadImage(&source, (u32*)g_menu_render_buffer);
    mem_copy_bytes_nonempty(&clear_strip, &source, sizeof(RECT));
    clear_strip.h = 2;
    clear_strip.x = source.x;
    clear_strip.y = ((source.y - 2) & 0xff) + (source.y & 0xff00);
    clear_strip.w = source.w;
    ClearImage(&clear_strip, 0, 0, 0);
    return g_menu_render_buffer;
}
