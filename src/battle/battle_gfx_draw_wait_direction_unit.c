#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

/*
 * Queue the acting unit's wait-direction indicator into the ordering table at
 * the depth of a point 48 screen units above its feet.
 */
void battle_gfx_draw_wait_direction_unit(void) {
    SVECTOR base;
    SVECTOR raised;
    VECTOR transformed;
    long flag;
    battle_unit_misc_data_t* unit;
    s32 depth;
    u32* ot;

    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    unit = battle_unit_get_source_misc_data();
    main_util_set_svector(&base, unit->screen.vx, unit->screen.vy, unit->screen.vz);
    RotTrans(&base, &transformed, &flag);
    battle_camera_convert_screen_coords_modify_by_1(&unit->screen.vx, (s16*)&raised);
    raised.vy -= 48;
    RotTrans(&raised, &transformed, &flag);
    depth = transformed.vz / 4;
    ot = main_gfx_get_otag();
    battle_gfx_draw_wait_direction_arrows(&base, &ot[depth]);
}
