#include "fft/world.h"
#include "psx/types.h"

/*
 * Compute the portrait index shown for a party unit.
 *
 * Generic male/female identities derive their sheet from the job; other
 * selectors take the job's spritesheet. Portraits of 0x3e and above are
 * expanded by the job's portrait palette.
 */
s32 world_unit_get_portrait_index(s32 party_index) {
    party_data_t* unit = main_party_get_data_pointer(party_index);
    job_data_t* job;
    s32 portrait;
    s32 product;
    s32 palette_base;

    if (unit->sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
            s32 base = (unit->job_id - JOB_ID_SQUIRE) * 2;
            portrait = base + 0x60;
            if (unit->job_id == JOB_ID_MIME) {
                portrait = base + 0x5E;
            }
        } else if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            s32 base = (unit->job_id - JOB_ID_SQUIRE) * 2;
            portrait = base + 0x61;
            if (unit->job_id == JOB_ID_MIME) {
                portrait = base + 0x5F;
            }
        } else {
            job = main_job_get_data_pointer(unit->job_id);
            portrait = job->spritesheet_id;
        }
        if (unit->job_id == JOB_ID_BARD) {
            portrait = 0x82;
        }
        if (unit->job_id == JOB_ID_DANCER) {
            portrait = 0x83;
        }
    } else {
        portrait = unit->sprite_set;
    }
    portrait = g_world_formation_sprite_by_portrait[portrait];
    if (unit->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        portrait = 0;
    }
    if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
        portrait |= job->job_portrait_palette << 10;
    }
    portrait &= 0xFF;
    if (portrait == 0x18) {
        return 0x7A;
    }
    if (portrait >= 0x3F) {
        portrait = g_world_monster_formation_sprite_table[portrait];
    } else if (unit->sprite_set & CHARACTER_IDENTITY_HIGH_BIT_MASK) {
        portrait -= 1;
    }
    if (portrait >= 0x3E) {
        portrait -= 0x3E;
        product = portrait * 3;
        palette_base = job->job_portrait_palette + 0x3E;
        portrait = product + palette_base;
    }
    return portrait;
}
