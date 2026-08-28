#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/main_sound.h"
#include "fft/option.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* option_scroll_layout_t lives in fft/option.h. */

/* Apply scroll input, rebuild the menu image, and upload the changed region. */
void* option_menu_update_scroll(u8* menu, s32* first_row, s32* render_pending) {
    RECT source;
    RECT clear_strip;
    option_scroll_layout_t* layout;
    u32* primary_input;
    u32* secondary_input;
    s32 changed;

    layout = *(option_scroll_layout_t**)(menu + 0x30);
    primary_input = battle_script_get_controller_input_pointer(0);
    secondary_input = battle_script_get_controller_input_pointer(1);
    source.x = *(u16*)(menu + 0);
    source.y = *(u16*)(menu + 2);
    source.w = *(s16*)(menu + 4) >> 2;
    source.h = *(u16*)(menu + 6);
    changed = 0;

    if (g_battle_menu_input_disabled != 0) {
        return 0;
    }

    if (((*secondary_input & PSX_PAD_SQUARE) == 0)
        && (*(s32*)((g_battle_current_thread_id * NATIVE_THREAD_STRIDE) + (s32)g_battle_threads + 0x60) == 0)) {
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
    } else if (*(s32*)((g_battle_current_thread_id * NATIVE_THREAD_STRIDE) + (s32)g_battle_threads + 0x60) != 0) {
        changed = 1;
        *(s32*)((g_battle_current_thread_id * NATIVE_THREAD_STRIDE) + (s32)g_battle_threads + 0x60) = 0;
    }

    if (changed == 0) {
        return 0;
    }

    battle_clear_menu_render_buffer(g_option_menu_render_buffer, (*(s16*)(menu + 4) * *(s16*)(menu + 6)) / 2);
    option_menu_render_entries((option_menu_entry_t*)menu, first_row, (void*)g_option_menu_render_buffer);
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
