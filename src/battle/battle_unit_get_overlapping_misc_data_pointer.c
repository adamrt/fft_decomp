#include "fft/battle.h"
#include "psx/types.h"

battle_unit_misc_data_t* battle_unit_get_overlapping_misc_data_pointer(
    battle_unit_misc_data_t* self, s32 x, s32 y, s32 z) {
    battle_unit_misc_data_t* list[16];
    battle_unit_misc_data_t* unit;
    battle_stats_t* stats;
    s32 count;
    s32 i;

    count = 0;
    unit = g_battle_unit_misc_list_head;
    if (unit != 0) {
        do {
            if (unit != self && unit->map_x == x && unit->map_y == y && unit->map_z == z) {
                stats = unit->battle_data;
                if (stats != 0 && stats->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                    if (self->mount_state == BATTLE_MISC_MOUNT_STATE_NONE
                        || unit->unit_id != self->mount_partner_misc_id) {
                        list[count] = unit;
                        count += 1;
                    }
                }
            }
            unit = unit->previous;
        } while (unit != 0);
    }
    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (list[i]->unit_id == g_casting_unit_misc_id) {
                return list[i];
            }
        }
        return list[0];
    }
    return 0;
}
