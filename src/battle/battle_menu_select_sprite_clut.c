#include "fft/battle.h"

/* Select the CLUT bank for a menu sprite: the highlighted palette while the
 * previous event thread is running or event mode is active, otherwise the
 * normal palette.
 *
 * Target 0x801383c8. Structurally the BATTLE twin of
 * world_menu_select_icon_cluts. */
void battle_menu_select_sprite_clut(SPRT* sprite) {
    if (battle_thread_is_previous_running() != 0 || g_event_mode == 1) {
        sprite->clut = 0x7D3C;
    } else {
        sprite->clut = 0x7C3C;
    }
}
