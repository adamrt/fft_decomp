#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/world.h"

void world_formation_dismiss_unit(s16 formation_idx) {
    s32 i;

    if (world_formation_can_dismiss_unit(formation_idx) != 0) {
        i = 0;
        do {
            world_formation_equip_item_to_unit_slot(formation_idx, i, 0);
            i += 1;
        } while (i < 5);
        main_party_remove_unit(g_world_formation_unit_pointers[formation_idx]->roster_slot);
    }
}
