#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_sync_all_rider_mount_positions(void) {
    s32 i = 0;
    do {
        battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_misc_id(i & 0xFFFF);
        if (unit) {
            battle_unit_set_rider_mount_x_y_facing(unit);
        }
        i++;
    } while (i < 0x10);
}
