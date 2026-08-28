#include "fft/battle.h"
#include "fft/job.h"

u8 main_unit_get_spritesheet_palette(battle_stats_t* unit, u8* palette) {
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
        sprite_set = job_id * 2 - 0x34;
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
        sprite_set = job_id * 2 - 0x33;
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_MONSTER) {
        *palette = unit->job_portrait_palette;
        return unit->spritesheet_id;
    }
    sprite_set = job_id + 0x28;
    return sprite_set;
}
