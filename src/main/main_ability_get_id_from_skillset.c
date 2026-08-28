#include "fft/main_unit.h"

typedef struct {
    u8 high_id_bits[3];
    u8 ability_ids[HUMAN_SKILLSET_ABILITY_COUNT];
} human_skillset_abilities_t;

typedef struct {
    u8 high_id_bits;
    u8 ability_ids[MONSTER_SKILLSET_ABILITY_COUNT];
} monster_skillset_abilities_t;

extern human_skillset_abilities_t g_main_ability_human_skillset_abilities[];
extern monster_skillset_abilities_t g_main_ability_monster_skillset_abilities[];

s32 main_ability_get_id_from_skillset(s32 skillset_id, s32 ability_index) {
    if (skillset_id < SKILLSET_ID_MONSTER_FIRST) {
        u8* table_base;
        u8* ability_entry;
        s32 adjusted_index;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= HUMAN_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        adjusted_index = ability_index;
        table_base = (u8*)g_main_ability_human_skillset_abilities;
        skillset_id = (s32)(table_base + skillset_id * sizeof(human_skillset_abilities_t));
        if (ability_index < 0) {
            adjusted_index = ability_index + 7;
        }
        flag_byte_index = adjusted_index >> 3;
        high_id_bit = ((u8*)skillset_id)[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        ability_entry = (u8*)skillset_id + ability_index;
        return ability_entry[3] | high_id_bit;
    }

    if (skillset_id < SKILLSET_ID_END) {
        u8* table_base;
        u8* ability_entry;
        s32 adjusted_index;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= MONSTER_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        adjusted_index = ability_index;
        table_base = (u8*)g_main_ability_monster_skillset_abilities
            - SKILLSET_ID_MONSTER_FIRST * sizeof(monster_skillset_abilities_t);
        skillset_id = (s32)(table_base + skillset_id * sizeof(monster_skillset_abilities_t));
        if (ability_index < 0) {
            adjusted_index = ability_index + 7;
        }
        flag_byte_index = adjusted_index >> 3;
        high_id_bit = ((u8*)skillset_id)[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        ability_entry = (u8*)skillset_id + ability_index;
        return ability_entry[1] | high_id_bit;
    }

    return 0;
}
