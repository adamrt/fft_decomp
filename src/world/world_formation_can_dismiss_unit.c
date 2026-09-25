#include "fft/world.h"

/* Ramza's sprite sets and guest roster slots cannot be
 * dismissed. */
s32 world_formation_can_dismiss_unit(s16 formation_idx) {
    world_formation_unit_t* unit = g_world_formation_unit_pointers[formation_idx];
    s32 sprite_set = unit->sprite_set;
    s32 roster_slot = unit->roster_slot;
    if (sprite_set <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
        return 0;
    return roster_slot < PARTY_GUEST_SLOT_FIRST;
}
