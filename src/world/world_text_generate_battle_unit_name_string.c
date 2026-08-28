#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "fft/world.h"

/* Battle-unit twin of world_text_generate_formation_unit_name_string: packs
 * the names of the 21 battle units into the name string buffer. */
void world_text_generate_battle_unit_name_string(void) {
    u8* out;
    battle_stats_t* unit;
    s32 i;
    s32 j;
    u8 character;

    world_text_init_section_pointers();
    out = g_world_text_roster_unit_names;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_get_stats_from_battle_id(i);
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            *out = 0xFE;
            out++;
        } else {
            for (j = 0; j < 0x10; j++) {
                character = unit->name[j];
                *out = character;
                if ((character & 0xFE) == 0xFE) {
                    out++;
                    break;
                }
                out++;
                if (j == 0xF) {
                    *out = 0xFE;
                    out++;
                }
            }
        }
    }
}
