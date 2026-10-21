/* Resident access to packed skillset, ability, item and job tables. Skillset decoding
 * expands the high ability-id bits into the scratch list used by unit generation and menus. */

#include "fft/ability.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/skillset.h"
#include "psx/types.h"

extern job_data_t g_job_data[JOB_ID_COUNT];

/* Packed skillset rows decoded by ability_skillset_id_get. */
typedef struct ability_human_skillset {
    u8 high_id_bits[3];
    u8 ability_ids[HUMAN_SKILLSET_ABILITY_COUNT];
} ability_human_skillset_t;

typedef struct ability_monster_skillset {
    u8 high_id_bits;
    u8 ability_ids[MONSTER_SKILLSET_ABILITY_COUNT];
} ability_monster_skillset_t;

extern ability_human_skillset_t g_ability_human_skillset_abilities[SKILLSET_ID_MONSTER_FIRST];
extern ability_monster_skillset_t g_ability_monster_skillset_abilities[SKILLSET_ID_END - SKILLSET_ID_MONSTER_FIRST];

/* Scratch list filled by ability_skillset_list_build. */
static u16 s_skillset_abilities[SKILLSET_ABILITY_LIST_COUNT] = { 0 };

/* Return the ability id in one slot of a skillset, or 0 for a slot or skillset
 * out of range.
 *
 * A human skillset (below SKILLSET_ID_MONSTER_FIRST) has 22 slots, a monster
 * skillset 4. Each record stores the low byte of every id; bit 8 of slot n comes
 * from the high_id_bits bytes, most significant bit first. */
s32 ability_skillset_id_get(s32 skillset_id, s32 ability_index) {
    if (skillset_id < SKILLSET_ID_MONSTER_FIRST) {
        ability_human_skillset_t* table_base;
        ability_human_skillset_t* skillset;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= HUMAN_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        table_base = g_ability_human_skillset_abilities;
        skillset = table_base + skillset_id;
        flag_byte_index = ability_index / 8;
        high_id_bit = skillset->high_id_bits[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        return skillset->ability_ids[ability_index] | high_id_bit;
    }

    if (skillset_id < SKILLSET_ID_END) {
        ability_monster_skillset_t* table_base;
        ability_monster_skillset_t* skillset;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= MONSTER_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        table_base = g_ability_monster_skillset_abilities - SKILLSET_ID_MONSTER_FIRST;
        skillset = table_base + skillset_id;
        flag_byte_index = ability_index / 8;
        high_id_bit = (&skillset->high_id_bits)[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        return skillset->ability_ids[ability_index] | high_id_bit;
    }

    return 0;
}

/* Fill g_skillset_abilities with the skillset's 24 ability ids and return
 * it. An id whose class (skillset_ability_filter_e) is not selected in filters
 * is written as 0. Skillset ids from 0x100 up read skillset 0. */
u16* ability_skillset_list_build(s32 skillset_id, s32 filters) {
    s32 output_count;
    s32 ability_index;
    u16 ability_id;
    s32 keep;

    if (skillset_id >= 0x100) {
        skillset_id = 0;
    }
    output_count = 0;
    for (ability_index = 0; ability_index < SKILLSET_ABILITY_LIST_COUNT; ability_index++) {
        ability_id = ability_skillset_id_get(skillset_id, ability_index);
        if (ability_id < ABILITY_ID_REACTION_FIRST) {
            keep = filters & SKILLSET_ABILITY_FILTER_ACTION;
        } else if (ability_id < ABILITY_ID_SUPPORT_FIRST) {
            keep = filters & SKILLSET_ABILITY_FILTER_REACTION;
        } else if (ability_id <= ABILITY_ID_MOVEMENT_FIRST) {
            /* One past ABILITY_ID_MOVEMENT_FIRST in the original. */
            keep = filters & SKILLSET_ABILITY_FILTER_SUPPORT;
        } else {
            keep = filters & SKILLSET_ABILITY_FILTER_MOVEMENT;
        }
        if (keep == 0) {
            ability_id = 0;
        }
        s_skillset_abilities[output_count++] = ability_id;
    }

    while (output_count < SKILLSET_ABILITY_LIST_COUNT) {
        s_skillset_abilities[output_count] = 0;
        output_count += 1;
    }
    return s_skillset_abilities;
}

/* Classify an ability id and return pointers to its two table records.
 *
 * *out_ability_data always receives the common 8-byte record. *out_secondary_data
 * receives the record of the id's range: the range/effect record of an ordinary
 * ability, the item id of an Item ability, the item type of a Throw ability, the
 * Jump or Charge record, or the one-byte record shared by Math, reaction, support
 * and movement abilities. The result is the matching ability_type_e. */
ability_type_e ability_records_get(
    s32 ability_id, const ability_data_t** out_ability_data, ability_secondary_pointer_t* out_secondary_data) {
    ability_id &= ABILITY_ID_MASK;
    /* 0x8005ebf0: Ability Data 1, 8 bytes each. */
    *out_ability_data = &g_ability_data[ability_id];
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        out_secondary_data->range = &g_ability_range_data[ability_id];
        return ABILITY_TYPE_DEFAULT;
    }
    if (ability_id < ABILITY_ID_THROW_FIRST) {
        out_secondary_data->item_id = &g_item_ability_item_ids[ability_id - ABILITY_ID_ITEM_FIRST];
        return ABILITY_TYPE_ITEM;
    }
    if (ability_id < ABILITY_ID_JUMP_FIRST) {
        out_secondary_data->throw_item_type = &g_ability_throw_item_types[ability_id - ABILITY_ID_THROW_FIRST];
        return ABILITY_TYPE_THROW;
    }
    if (ability_id < ABILITY_ID_CHARGE_FIRST) {
        out_secondary_data->jump = &g_ability_jump_data[ability_id - ABILITY_ID_JUMP_FIRST];
        return ABILITY_TYPE_JUMP;
    }
    if (ability_id < ABILITY_ID_MATH_FIRST) {
        out_secondary_data->charge = &g_ability_charge_data[ability_id - ABILITY_ID_CHARGE_FIRST];
        return ABILITY_TYPE_CHARGE;
    }
    if (ability_id < ABILITY_ID_REACTION_FIRST) {
        out_secondary_data->math_rsm = &g_ability_math_rsm_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_MATH;
    }
    if (ability_id < ABILITY_ID_SUPPORT_FIRST) {
        out_secondary_data->math_rsm = &g_ability_math_rsm_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_REACTION;
    }
    if (ability_id < ABILITY_ID_MOVEMENT_FIRST) {
        out_secondary_data->math_rsm = &g_ability_math_rsm_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_SUPPORT;
    }
    out_secondary_data->math_rsm = &g_ability_math_rsm_data[ability_id - ABILITY_ID_MATH_FIRST];
    return ABILITY_TYPE_MOVEMENT;
}

/* Return an item's primary data record; the id is masked to its low byte, so it never fails. */
const item_data_t* item_data_get(s32 item_id) {
    return &g_item_primary_data[item_id & ITEM_ID_MASK];
}

/* Return a job's data record, or NULL for ids at or past JOB_ID_COUNT. */
const job_data_t* job_data_get(s32 job_id) {
    if (job_id < JOB_ID_COUNT) {
        return &g_job_data[job_id];
    }
    return 0;
}
