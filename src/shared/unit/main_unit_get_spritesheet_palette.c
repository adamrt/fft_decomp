#include "fft/main.h"

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
