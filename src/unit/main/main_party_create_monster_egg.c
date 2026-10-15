#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Add an egg of monster_job to the first empty ordinary roster slot. Returns the
 * slot, or -1 when there is none.
 *
 * egg_modifier is stored as the unit's birthday (values from 0x16e up become 1)
 * and the zodiac sign is derived from it; the breeding code passes the same value
 * in both nibbles. */
s32 main_party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color) {
    s32 party_index;
    party_unit_t* party;
    u16 birthday;
    s32 zodiac;

    party_index = main_party_generate_unit(MAIN_UNIT_TYPE_MONSTER);
    if (party_index == -1) {
        return party_index;
    }
    party = main_party_get_unit(party_index);
    party->job_id = monster_job;
    party->egg_color = egg_color;
    party->gender_flags |= UNIT_FLAG_EGG;
    birthday = egg_modifier;
    if (birthday >= 0x16e) {
        birthday = 1;
    }
    zodiac = main_unit_calculate_zodiac_symbol(birthday);
    party->birthday_day = birthday;
    party->zodiac = ((birthday & 0x100) >> 8) + (zodiac << 4);
    return party_index;
}
