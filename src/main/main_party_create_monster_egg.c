#include "fft/main.h"

s32 main_party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color) {
    s32 party_index;
    party_data_t* party_data;
    s32 birthday;
    s32 zodiac;

    party_index = main_party_generate_unit(MAIN_UNIT_TYPE_MONSTER);
    if (party_index == -1) {
        return party_index;
    }
    party_data = main_party_get_data_pointer(party_index);
    party_data->job_id = monster_job;
    party_data->egg_color = egg_color;
    party_data->gender_flags |= UNIT_FLAG_EGG;
    birthday = egg_modifier;
    if ((u16)birthday >= 0x16e) {
        birthday = 1;
    }
    zodiac = main_unit_calculate_zodiac_symbol((u16)birthday);
    party_data->birthday_day = birthday;
    party_data->zodiac = (((u32)birthday & 0x100) >> 8) + ((u32)zodiac << 4);
    return party_index;
}
