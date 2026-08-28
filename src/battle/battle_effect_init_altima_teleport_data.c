#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_init_altima_teleport_data(battle_unit_misc_data_t* unit, u8* teleport_data) {
    u8 misc_id;
    u8 repeated_misc_id;

    *(s16*)&teleport_data[0xa4] = 0;
    misc_id = unit->unit_id;
    *(s16*)&teleport_data[0] = 1;
    *(s16*)&teleport_data[4] = 0;
    teleport_data[0xa6] = misc_id;
    repeated_misc_id = unit->unit_id;
    teleport_data[2] = 0;
    teleport_data[6] = repeated_misc_id;
}
