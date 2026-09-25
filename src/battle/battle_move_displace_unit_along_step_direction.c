#include "fft/battle.h"
#include "psx/types.h"

void battle_move_displace_unit_along_step_direction(battle_unit_misc_data_t* unit, s32 x, s32 y, s32 delta) {
    s16 axis;
    s16 screen_z;

    if ((unit->status_flags_1_4 & BATTLE_MISC_STATUS_DEAD) != 0) {
        return;
    }
    if ((unit->status_flags_5_6 & (BATTLE_MISC_STATUS_CRYSTAL | BATTLE_MISC_STATUS_TREASURE | BATTLE_MISC_STATUS_JUMP))
        != 0) {
        return;
    }
    switch (g_battle_move_displacement_direction) {
    case 0:
        axis = y - delta;
        unit->screen.vz = axis;
        unit->real.vz = axis << 12;
        break;
    case 2:
        axis = y + delta;
        unit->screen.vz = axis;
        unit->real.vz = axis << 12;
        break;
    case 1:
        axis = x + delta;
        unit->screen.vx = axis;
        unit->real.vx = axis << 12;
        break;
    case 3:
        axis = x - delta;
        unit->screen.vx = axis;
        unit->real.vx = axis << 12;
        break;
    }
    screen_z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
    unit->screen.vy = screen_z;
    unit->real.vy = screen_z << 12;
    unit->shadow_dirty |= 1;
}
