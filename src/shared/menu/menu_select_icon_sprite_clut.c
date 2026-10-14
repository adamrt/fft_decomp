#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"

/* Select a menu sprite's CLUT as battle_menu_select_icon_cluts does for its fourth
 * sprite: the dimmed one while the previous thread is running or g_menu_input_lock_mode is 1. */
void menu_select_icon_sprite_clut(SPRT* sprite) {
    s32 clut;

    if (battle_thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
        clut = 0x7c7c;
    } else {
        clut = 0x7cbc;
    }
    sprite->clut = clut;
}
