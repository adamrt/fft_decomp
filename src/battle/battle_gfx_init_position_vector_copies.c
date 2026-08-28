#include "fft/battle.h"

void battle_gfx_init_position_vector_copies(battle_unit_misc_data_t* unit) {
    s32 offset;
    s32 i;

    unit->position_copies_active = 1;
    i = 0;
    /* The retail loop walks display_svectors by an integer offset
     * (li 0x2ec / addiu 8); a typed SVECTOR pointer emits addiu from unit. */
    offset = 0x2ec;
    do {
        main_util_set_svector((SVECTOR*)((u8*)unit + offset), unit->screen.vx, unit->screen.vy, unit->screen.vz);
        offset += 8;
        i++;
    } while (i < 6);
}
