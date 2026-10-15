#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Empty. battle_unit_remove passes the removed unit's sprite display, a member of its
 * g_battle_units record that battle_gfx_init_unit_sprite_display handed out. */
void battle_gfx_release_unit_sprite_display_stub(battle_gfx_sprite_display_data_t* unused_display) {
}
