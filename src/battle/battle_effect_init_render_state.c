#include "fft/battle.h"

#define EFFECT_PRIM_BUFFER_SIZE 0x8000
#define EFFECT_CORNER_Z_DEFAULT 0x200

/* Initialize the effect renderer's transform, depth, and primitive state.
 *
 * The rotation terms form the identity matrix's third row and column. Every
 * corner starts at the same screen depth before an effect supplies geometry. */
void battle_effect_init_render_state(void) {
    g_battle_effect_matrix.m[2][2] = ONE;
    g_battle_effect_frame_parity = 0;
    g_battle_effect_matrix.m[2][1] = 0;
    g_battle_effect_matrix.m[2][0] = 0;
    g_battle_effect_matrix.m[1][2] = 0;
    g_battle_effect_matrix.m[0][2] = 0;
    g_battle_effect_matrix.t[2] = 0;
    g_battle_effect_corner_bottom_right.vz = EFFECT_CORNER_Z_DEFAULT;
    g_battle_effect_corner_bottom_left.vz = EFFECT_CORNER_Z_DEFAULT;
    g_battle_effect_corner_top_right.vz = EFFECT_CORNER_Z_DEFAULT;
    g_battle_effect_corner_top_left.vz = EFFECT_CORNER_Z_DEFAULT;
    g_battle_effect_prim_buffer_offset = 0;
    g_battle_effect_frame_start_prim_buffer_offset = 0;
    g_battle_effect_prim_buffer = battle_heap_alloc_block(EFFECT_PRIM_BUFFER_SIZE, 0);
    g_battle_effect_sprite_count_peak = 0;
    g_battle_effect_sprite_count = 0;
}
