#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/main_sound.h"
#include "fft/option.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* option_scroll_layout_t lives in fft/option.h. */

/* Apply scroll input, rebuild the menu image, and upload the changed region. */
void* option_menu_update_scroll(option_menu_entry_t* menu, s32* first_row, s32* render_pending) {
    RECT source;
    RECT clear_strip;
    option_scroll_layout_t* layout;
    u32* primary_input;
    u32* secondary_input;
    s32 changed;

    layout = (option_scroll_layout_t*)menu->text_binding;
    primary_input = battle_script_get_controller_input_pointer(0);
    secondary_input = battle_script_get_controller_input_pointer(1);
    source.x = menu->vram_x;
    source.y = menu->vram_y;
    source.w = menu->inner_width >> 2;
    source.h = menu->inner_height;
    changed = 0;

    if (g_battle_menu_input_disabled != 0) {
        return 0;
    }

    if (((*secondary_input & PSX_PAD_SQUARE) == 0)
        && (g_battle_threads[g_battle_current_thread_id].task_words[4] == 0)) {
        return 0;
    }

    if ((*primary_input & PSX_PAD_UP) != 0 && *first_row != 0) {
        *first_row -= layout->step;
        if (*first_row < 0) {
            *first_row = 0;
        }
        changed = 1;
        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
    } else if ((*primary_input & PSX_PAD_DOWN) != 0 && *first_row != layout->maximum) {
        *first_row += layout->step;
        if (*first_row > layout->maximum) {
            *first_row = layout->maximum;
        }
        changed = 1;
        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
    } else if (g_battle_threads[g_battle_current_thread_id].task_words[4] != 0) {
        changed = 1;
        g_battle_threads[g_battle_current_thread_id].task_words[4] = 0;
    }

    if (changed == 0) {
        return 0;
    }

    battle_clear_menu_render_buffer(g_option_menu_render_buffer, (menu->inner_width * *(s16*)&menu->inner_height) / 2);
    option_menu_render_entries(menu, first_row, (void*)g_option_menu_render_buffer);
    *render_pending = 0;
    LoadImage(&source, (u32*)g_option_menu_render_buffer);
    battle_copy_bytes(&clear_strip, &source, sizeof(RECT));
    clear_strip.h = 2;
    clear_strip.x = source.x;
    clear_strip.y = ((source.y - 2) & 0xff) + (source.y & 0xff00);
    clear_strip.w = source.w;
    ClearImage(&clear_strip, 0, 0, 0);
    return g_option_menu_render_buffer;
}
