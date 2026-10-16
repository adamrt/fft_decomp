/* Texturing a quad with a roster unit's portrait, its CLUT found from the unit's sprite sheet. */

#include "fft/data_job.h"
#include "fft/main_unit.h"
#include "fft/menu_panel.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Resolves a roster slot's sprite sheet to its 16-bit portrait code and,
 * unless the code's suppress bits are set, textures `poly` with that portrait.
 *
 * Special characters index the portrait table by their sprite set; generic
 * units by job and gender, Bard and Dancer included; monsters by their job's
 * sprite sheet, with the job's portrait palette added from bit 10. An empty
 * slot (no sprite set) gets code 0.
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
            sprite = offset + JOB_SHEET_GENERIC_MALE_BASE;
            if (job_id == JOB_ID_MIME) {
                sprite = offset + JOB_SHEET_GENERIC_MALE_MIME_BASE;
            }
        } else if (sprite == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            job_id = party->job_id;
            offset = (job_id - JOB_ID_GENERIC_FIRST) * 2;
            sprite = offset + JOB_SHEET_GENERIC_FEMALE_BASE;
            if (job_id == JOB_ID_MIME) {
                sprite = offset + JOB_SHEET_GENERIC_FEMALE_MIME_BASE;
            }
        } else {
            job = main_job_get_data_pointer(party->job_id);
            sprite = job->spritesheet_id;
        }
        current_job = party->job_id;
        if (current_job == JOB_ID_BARD) {
            sprite = JOB_SHEET_BARD;
        }
        if (current_job == JOB_ID_DANCER) {
            sprite = JOB_SHEET_DANCER;
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
