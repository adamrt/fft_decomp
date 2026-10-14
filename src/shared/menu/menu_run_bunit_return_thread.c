#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Run BUNIT, release its high-overlay memory, then restore the unit view. */
void menu_run_bunit_return_thread(void) {
    s32 i;
    s32 unused[2]; /* keeps the original 0x28 frame */

    g_world_menu_transition_timeout = 4;
    do {
        thread_yield();
    } while (world_thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0);

    g_world_companion_overlay_state = COMPANION_OVERLAY_STATE_BUNIT;
    g_world_text_overlay_active = 0;
    do {
        thread_wait_frames(1);
    } while (g_world_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);

    menu_free_high_overlay();
    for (i = 2; i < 15; i++) {
        thread_set_parameters(i, 0, 0, 1);
    }
    for (i = 0; i < 0x100; i += 8) {
        g_gfx_screen_fade = 0xFF - i;
        thread_yield();
    }
    menu_clear_modal_open();
    g_menu_overlay_state = 0;
    g_gfx_screen_fade = 0;
    g_world_menu_transition_timeout = 0;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_HOVERED_UNIT, g_menu_selected_unit.battle_id, 0xFF);
    thread_exit_current();
}
