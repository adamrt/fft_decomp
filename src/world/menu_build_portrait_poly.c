/* Texturing a quad with a roster unit's portrait, its CLUT found from the unit's sprite sheet. */

#include "fft/data_job.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Generic units take their sheet from the job: two sheets (male, female) per
 * generic job id from JOB_ID_GENERIC_FIRST, with Mime, Bard and Dancer apart. */
#define SHEET_GENERIC_MALE_BASE   0x60
#define SHEET_GENERIC_MALE_MIME   0x5E
#define SHEET_GENERIC_FEMALE_BASE 0x61
#define SHEET_GENERIC_FEMALE_MIME 0x5F
#define SHEET_BARD                0x82
#define SHEET_DANCER              0x83

/*
 * Resolves a roster slot's sprite sheet to its 16-bit CLUT value and, unless that value
 * is masked out, applies it to the caller's portrait sprite record.
 */
void menu_build_portrait_poly(s32 roster_slot, POLY_FT4* poly) {
    party_unit_t* party = main_party_get_unit(roster_slot);
    job_data_t* job;
    u32 sprite = party->sprite_set;
    u8 job_id;
    s32 offset;
    s32 current_job;

    if (sprite >= CHARACTER_IDENTITY_GENERIC_MALE) {
        if (sprite == CHARACTER_IDENTITY_GENERIC_MALE) {
            job_id = party->job_id;
            offset = (job_id - JOB_ID_GENERIC_FIRST) * 2;
            sprite = offset + SHEET_GENERIC_MALE_BASE;
            if (job_id == JOB_ID_MIME) {
                sprite = offset + SHEET_GENERIC_MALE_MIME;
            }
        } else if (sprite == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            job_id = party->job_id;
            offset = (job_id - JOB_ID_GENERIC_FIRST) * 2;
            sprite = offset + SHEET_GENERIC_FEMALE_BASE;
            if (job_id == JOB_ID_MIME) {
                sprite = offset + SHEET_GENERIC_FEMALE_MIME;
            }
        } else {
            job = main_job_get_data_pointer(party->job_id);
            sprite = job->spritesheet_id;
        }
        current_job = party->job_id;
        if (current_job == JOB_ID_BARD) {
            sprite = SHEET_BARD;
        }
        if (current_job == JOB_ID_DANCER) {
            sprite = SHEET_DANCER;
        }
    }
    sprite = g_world_portrait_clut_by_sheet_id[sprite];
    if (party->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        sprite = 0;
    }
    if (party->sprite_set == CHARACTER_IDENTITY_MONSTER) {
        sprite |= job->job_portrait_palette << 10;
    }
    if ((sprite & MENU_PORTRAIT_CLUT_SUPPRESS_MASK) == 0) {
        menu_build_unit_portrait_poly(poly, sprite);
    }
}
