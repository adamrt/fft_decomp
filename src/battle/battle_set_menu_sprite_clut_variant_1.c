#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/gpu.h"

/* Select the menu-sprite palette row for the active thread context. */
void battle_set_menu_sprite_clut_variant_1(SPRT* sprite) {
    s32 clut;

    if (battle_thread_is_previous_running() != 0 || g_event_mode == 1) {
        clut = 0x7c7c;
    } else {
        clut = 0x7cbc;
    }
    sprite->clut = clut;
}
