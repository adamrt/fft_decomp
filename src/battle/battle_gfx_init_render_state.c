#include "fft/battle.h"
#include "fft/battle_gfx.h"

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
    /* The in-use word just past the last g_battle_unit_misc_data record. */
    g_battle_unit_last_misc_init_byte = &g_battle_unit_misc_slot_flags[16].in_use;
}
