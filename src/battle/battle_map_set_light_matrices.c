#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

/* Main-executable libgte routine at 0x8001d108; it loads the matrix into GTE
 * control registers 16-20 (the colour matrix), like Psy-Q SetColorMatrix. */

/*
 * Copy two matrices into the working pair at g_battle_map_light_color_matrix and into entries 0 and
 * 2 of g_battle_map_lightning_state.color_matrices, then load the first into the GTE colour matrix.
 *
 * Each backup copy rereads the working field; a chained assignment assigns the
 * loop pointers to different registers.
 */
void battle_map_set_light_matrices(MATRIX* first, MATRIX* second) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_battle_map_light_color_matrix.m[i][j] = first->m[i][j];
            g_battle_map_lightning_state.color_matrices[0].m[i][j] = g_battle_map_light_color_matrix.m[i][j];
        }
        g_battle_map_light_color_matrix.t[i] = first->t[i];
        g_battle_map_lightning_state.color_matrices[0].t[i] = g_battle_map_light_color_matrix.t[i];
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_battle_map_light_matrix.m[i][j] = second->m[i][j];
            g_battle_map_lightning_state.color_matrices[2].m[i][j] = g_battle_map_light_matrix.m[i][j];
        }
        g_battle_map_light_matrix.t[i] = second->t[i];
        g_battle_map_lightning_state.color_matrices[2].t[i] = g_battle_map_light_matrix.t[i];
    }
    SetColorMatrix(&g_battle_map_light_color_matrix);
}
