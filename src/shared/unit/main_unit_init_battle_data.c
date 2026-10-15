#include "fft/main.h"
#include "fft/memory.h"

/*
 * Fills an in-battle unit record from its persistent party record.
 */
void main_unit_init_battle_data(unit_stats_t* unit, const party_unit_t* party) {
    s32 team;
    s32 i;
    s32 value;

    team = 8;
    if (party->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
        team = 0xb;
    }
    unit->team_flags = team;
    unit->initial_team_flags = team;
    unit->character_identity = party->sprite_set;
    unit->formation_index = party->party_id;
    unit->job_id = party->job_id;
    value = party->palette;
    if (party->party_id < PARTY_MEMBER_SLOT_COUNT) {
        value = 0;
    }
    unit->sprite_palette = value;
    unit->unit_flags = party->gender_flags;
    value = party->birthday_day + (party->zodiac << 8);
    unit->birthday.birthday = value;
    unit->birthday.zodiac = party->zodiac >> PARTY_ZODIAC_SHIFT;
    unit->secondary_skillset = party->secondary_skillset;
    unit->abilities.reaction = party->reaction_ability[0] + (party->reaction_ability[1] << 8);
    unit->abilities.support = party->support_ability[0] + (party->support_ability[1] << 8);
    unit->abilities.movement = party->movement_ability[0] + (party->movement_ability[1] << 8);
    mem_copy_byte_data_source_first(party->equipment, unit->equipment, UNIT_EQUIPMENT_SLOT_COUNT);
    unit->experience = party->experience;
    unit->level = party->level;
    unit->original_brave = unit->brave = party->bravery;
    unit->original_faith = unit->faith = party->faith;
    mem_copy_byte_data_source_first(party->raw_stats, unit->raw_stats, sizeof(unit->raw_stats));
    for (i = 0; i < UNIT_UNLOCKED_JOB_BYTE_COUNT; i++) {
        unit->career.unlocked_jobs[i] = party->career.unlocked_jobs[i];
    }
    mem_copy_byte_data_source_first(
        party->career.learned_abilities, unit->career.learned_abilities, UNIT_LEARNED_ABILITY_BYTE_COUNT);
    mem_copy_byte_data_source_first(party->career.job_levels, unit->career.job_levels, UNIT_JOB_LEVEL_BYTE_COUNT);
    /* Copy the adjacent current/total JP tables as their 80 serialized bytes. */
    mem_copy_byte_data_source_first(
        &party->career.job_points, &unit->career.job_points, sizeof(unit->career.job_points));
    mem_copy_byte_data_source_first(party->name, unit->name, sizeof(unit->name));
    unit->quote_name_id.value = party->name_id.fields.index + (party->name_id.fields.name_class << 8);
    mem_zero(&unit->ai_target_x, 7);
}
