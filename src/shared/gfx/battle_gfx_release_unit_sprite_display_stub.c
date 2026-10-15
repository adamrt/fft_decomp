#include "fft/battle_gfx.h"

/* Empty. battle_unit_remove passes the removed unit's sprite display, a member of its
 * g_battle_units record that battle_gfx_init_unit_sprite_display handed out. */
void battle_gfx_release_unit_sprite_display_stub(battle_gfx_sprite_display_data_t* unused_display) {
}
