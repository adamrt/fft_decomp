#include "fft/main.h"

u32 main_ability_get_known(const unit_stats_t* unit, u32 skillset_id) {
    return (unit->career.learned_abilities[skillset_id][0] << 16)
        + (unit->career.learned_abilities[skillset_id][1] << 8)
        | unit->career.learned_abilities[skillset_id][2];
}

/* Aggregate assignment changes the retail byte-copy loop. */
void main_util_copy_action_data(const action_command_t* source, action_command_t* destination) {
    const u8* from = (const u8*)source;
    u8* to = (u8*)destination;
    u32 index;

    for (index = 0; index < sizeof(*destination); index++) {
        *to++ = *from++;
    }
}
