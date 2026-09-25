#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Sprite-set ids above the ordinary sheet range select a generated sheet. */
#define SPRITE_SET_GENERIC_MALE   0x80
#define SPRITE_SET_GENERIC_FEMALE 0x81
#define SPRITE_SET_JOB_PORTRAIT   0x82

/* Monster job ids index the generated-sheet table from JOB_MONSTER_FIRST. */
#define JOB_MONSTER_FIRST 0x4A
#define JOB_BYBLOS        0x5B
#define JOB_STEEL_GIANT   0x5C
#define JOB_ULTIMA_DEMON  0x5D

#define SHEET_GENERIC_MALE_BASE     0x60
#define SHEET_GENERIC_MALE_ULTIMA   0x5E
#define SHEET_GENERIC_FEMALE_BASE   0x61
#define SHEET_GENERIC_FEMALE_ULTIMA 0x5F
#define SHEET_BYBLOS                0x82
#define SHEET_STEEL_GIANT           0x83

#define CLUT_SUPPRESS_MASK 0x300

/*
 * Resolves a roster slot's sprite sheet to its CLUT word and, unless the word
 * is masked out, applies it to the caller's portrait sprite record.
 */
void world_build_portrait_poly_ft4(s32 roster_slot, POLY_FT4* poly) {
    party_data_t* party;
    job_data_t* job;
    u32 sprite;
    u8 job_id;
    s32 offset;
    s32 current_job;

    party = main_party_get_data_pointer(roster_slot);
    sprite = party->sprite_set;
    if (sprite >= SPRITE_SET_GENERIC_MALE) {
        if (sprite == SPRITE_SET_GENERIC_MALE) {
            job_id = party->job_id;
            offset = (job_id - JOB_MONSTER_FIRST) * 2;
            sprite = offset + SHEET_GENERIC_MALE_BASE;
            if (job_id == JOB_ULTIMA_DEMON) {
                sprite = offset + SHEET_GENERIC_MALE_ULTIMA;
            }
        } else if (sprite == SPRITE_SET_GENERIC_FEMALE) {
            job_id = party->job_id;
            offset = (job_id - JOB_MONSTER_FIRST) * 2;
            sprite = offset + SHEET_GENERIC_FEMALE_BASE;
            if (job_id == JOB_ULTIMA_DEMON) {
                sprite = offset + SHEET_GENERIC_FEMALE_ULTIMA;
            }
        } else {
            job = main_job_get_data_pointer(party->job_id);
            sprite = job->spritesheet_id;
        }
        current_job = party->job_id;
        if (current_job == JOB_BYBLOS) {
            sprite = SHEET_BYBLOS;
        }
        if (current_job == JOB_STEEL_GIANT) {
            sprite = SHEET_STEEL_GIANT;
        }
    }
    sprite = g_world_portrait_clut_by_sheet_id[sprite];
    if (party->sprite_set == 0) {
        sprite = 0;
    }
    if (party->sprite_set == SPRITE_SET_JOB_PORTRAIT) {
        sprite |= job->job_portrait_palette << 10;
    }
    if ((sprite & CLUT_SUPPRESS_MASK) == 0) {
        world_gfx_set_portrait_poly_texture(poly, sprite);
    }
}
