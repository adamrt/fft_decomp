#include "fft/world.h"
#include "psx/types.h"

/* Resolve a party member's formation sprite word: bits 0-9 select the
 * sprite, and for monsters bits 10-11 carry the job's portrait palette. */
s32 world_get_party_unit_formation_sprite(s32 party_index) {
    party_data_t* party_unit;
    job_data_t* job_data;
    s32 sprite_set;
    s32 portrait_id;
    s32 formation_sprite;

    party_unit = main_party_get_data_pointer(party_index);
    sprite_set = party_unit->sprite_set;
    portrait_id = sprite_set;
    if ((u32)sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        if (sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
            portrait_id = (party_unit->job_id - JOB_ID_SQUIRE) * 2 + 0x60;
            if (party_unit->job_id == JOB_ID_MIME) {
                portrait_id -= 2;
            }
        } else if (sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            portrait_id = (party_unit->job_id - JOB_ID_SQUIRE) * 2 + 0x61;
            if (party_unit->job_id == JOB_ID_MIME) {
                portrait_id -= 2;
            }
        } else {
            job_data = main_job_get_data_pointer(party_unit->job_id);
            portrait_id = job_data->spritesheet_id;
        }
        if (party_unit->job_id == JOB_ID_BARD) {
            portrait_id = 0x82;
        }
        if (party_unit->job_id == JOB_ID_DANCER) {
            portrait_id = 0x83;
        }
    }

    formation_sprite = g_world_formation_sprite_by_portrait[portrait_id];
    sprite_set = party_unit->sprite_set;
    if (sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        formation_sprite = 0;
    }
    if (sprite_set == CHARACTER_IDENTITY_MONSTER) {
        formation_sprite |= job_data->job_portrait_palette << 10;
    }
    return formation_sprite;
}
