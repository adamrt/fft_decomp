/* Spritesheet/palette resolution with roll and status-set tests. */

#include "fft/main.h"

u8 main_unit_calculate_palette_spritesheet(unit_stats_t* unit, u8* palette) {
    u8 spritesheet = main_unit_get_spritesheet_palette(unit, palette);

    unit->spritesheet_id = spritesheet;
    unit->job_portrait_palette = *palette;
    return spritesheet;
}

/* Roll 0..maximum-1 and compare it with threshold: returns 0 when the roll is
 * below threshold and 1 otherwise. An event tested with `== 0` therefore has
 * threshold-in-maximum odds. */
s32 main_util_roll_pass_fail(s32 maximum, s32 threshold) {
    s32 roll = rand() * maximum / 0x8000;

    return roll >= threshold;
}

/* Resolve a unit's character identity to a spritesheet id; *palette is 0 except
 * for a monster.
 *
 * An identity below 0x80 is its own spritesheet. The generic male and female
 * selectors give the job's sheet, 0x60 + (job - JOB_ID_SQUIRE) * 2 plus one for a
 * female, with Dancer and Mime counted one job lower. The monster selector
 * returns the spritesheet and palette already in the unit (from its job record).
 * Any higher identity gives job_id + 0x28. */
u8 main_unit_get_spritesheet_palette(const unit_stats_t* unit, u8* palette) {
    u8 job_id;
    u8 sprite_set;

    *palette = 0;
    job_id = unit->job_id;
    sprite_set = unit->character_identity;
    if ((job_id == JOB_ID_DANCER) || (job_id == JOB_ID_MIME)) {
        job_id--;
    }
    if (sprite_set < CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
        sprite_set = job_id * 2 - (JOB_ID_SQUIRE * 2 - CHARACTER_IDENTITY_FIXED_MALE_SQUIRE);
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
        sprite_set = job_id * 2 - (JOB_ID_SQUIRE * 2 - CHARACTER_IDENTITY_FIXED_FEMALE_SQUIRE);
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_MONSTER) {
        *palette = unit->job_portrait_palette;
        return unit->spritesheet_id;
    }
    sprite_set = job_id + 0x28;
    return sprite_set;
}

s32 main_unit_has_status_in_set(const unit_stats_t* unit, main_status_check_set_e status_set_index) {
    const u8* status_set = g_main_status_check_sets[0];
    s32 index = 0;

    status_set += status_set_index * STATUS_BYTE_COUNT;

    for (; index < STATUS_BYTE_COUNT; index++) {
        if ((unit->status_sets.current[index] & status_set[index]) != 0) {
            return 1;
        }
    }
    return 0;
}
