#include "fft/battle.h"

void battle_state_handle_free_cursor_input(void) {
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_target_calculate_tile_coords_and_glow_from_at_list();
}
