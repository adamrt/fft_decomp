#include "fft/battle.h"
#include "fft/battle_gfx.h"

/* One-time setup of the sprite renderer: the numeric display matrix's third row
 * and column, a depth of 0x200 for the four GTE input vertices the part quads are
 * built from, the primitive ring index, and the primitive pool, which starts
 * right after the sixteen unit unit records. */
void battle_gfx_init_render_state(void) {
    g_battle_gfx_numeric_display_matrix.m[2][2] = ONE;
    g_battle_gte_rtps_vxy0.vz = 0x200;
    g_battle_gte_rtpt_vxy2.vz = 0x200;
    g_battle_gte_rtpt_vxy1.vz = 0x200;
    g_battle_gte_rtpt_vxy0.vz = 0x200;
    g_battle_gfx_numeric_display_matrix.m[2][1] = 0;
    g_battle_gfx_numeric_display_matrix.m[2][0] = 0;
    g_battle_gfx_numeric_display_matrix.m[1][2] = 0;
    g_battle_gfx_numeric_display_matrix.m[0][2] = 0;
    g_battle_gfx_numeric_display_matrix.t[2] = 0;
    g_battle_gfx_counter = 0;
    g_battle_gfx_previous_counter = 0;
    D_800b6290 = 0;
    D_800b628c = 0;
    /* The in-use 32-bit value just past the last g_battle_units record. */
    g_battle_gfx_sprite_prim_pool = (POLY_FT4*)&g_battle_unit_slot_states[BATTLE_UNIT_RUNTIME_SLOT_COUNT].in_use;
}
