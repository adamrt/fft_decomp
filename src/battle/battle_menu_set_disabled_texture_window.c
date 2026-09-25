#include "fft/battle.h"
#include "psx/types.h"

/* Clear the cursor sprite's width and install the disabled texture window. */
void battle_menu_set_disabled_texture_window(u8* menu) {
    /* Callers pass records with a shared DR_MODE, SPRT, and texture window RECT layout. */
    world_gfx_texture_window_record_t* record = (world_gfx_texture_window_record_t*)menu;

    record->sprite.w = 0;
    record->texture_window.h = 0;
    battle_copy_bytes(&record->texture_window, &g_battle_menu_disabled_texture_window, 8);
}
