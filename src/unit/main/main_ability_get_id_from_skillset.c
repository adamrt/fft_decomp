#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Return the ability id in one slot of a skillset, or 0 for a slot or skillset
 * out of range.
 *
 * A human skillset (below SKILLSET_ID_MONSTER_FIRST) has 22 slots, a monster
 * skillset 4. Each record stores the low byte of every id; bit 8 of slot n comes
 * from the high_id_bits bytes, most significant bit first. */
s32 main_ability_get_id_from_skillset(s32 skillset_id, s32 ability_index) {
    if (skillset_id < SKILLSET_ID_MONSTER_FIRST) {
        main_ability_human_skillset_t* table_base;
        main_ability_human_skillset_t* skillset;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= HUMAN_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        table_base = g_main_ability_human_skillset_abilities;
        skillset = table_base + skillset_id;
        flag_byte_index = ability_index / 8;
        high_id_bit = skillset->high_id_bits[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        return skillset->ability_ids[ability_index] | high_id_bit;
    }

    if (skillset_id < SKILLSET_ID_END) {
        main_ability_monster_skillset_t* table_base;
        main_ability_monster_skillset_t* skillset;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= MONSTER_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        table_base = g_main_ability_monster_skillset_abilities - SKILLSET_ID_MONSTER_FIRST;
        skillset = table_base + skillset_id;
        flag_byte_index = ability_index / 8;
        high_id_bit = (&skillset->high_id_bits)[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        return skillset->ability_ids[ability_index] | high_id_bit;
    }

    return 0;
}
