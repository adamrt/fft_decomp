#include "fft/main.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Mark an ability as learned in the unit's job-specific bitmap.
 *
 * The filtered skillset position determines the bitmap byte and bit.
 */
void world_ability_learn(s16 unit_id, s16 job_id, s16 ability_id) {
    s32 skillset;
    s16 generic_job;
    s16* list;
    s32 i;
    s32 byte;
    s32 bit;
    s32 mask;
    u8* learned_abilities;

    skillset = job_get_skillset(job_id);
    generic_job = job_get_generic_index(job_id);
    list = (s16*)main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);
    i = 0;
    while (list[i] != ability_id) {
        i++;
    }
    byte = i >> 3;
    bit = 7 - (i & 7);
    mask = 1;
    i = mask << bit;
    /* Keeping the job-byte stride before the field address preserves the retail load/add order. */
    learned_abilities = ((world_unit_t*)((u8*)g_world_unit_pointers[unit_id] + generic_job * 3))->learned_abilities[0];
    learned_abilities[byte] |= i;
}

/* Build the portrait source rectangle and palette for a formation unit.
 *
 * Special jobs use fixed portrait entries; generic humans derive theirs from
 * job and gender. The third argument is retained because all known callers
 * pass it even though this function does not consume it. */
void world_formation_build_unit_graphic_entry(s16 formation_index, texture_source_t* destination, s32 unused) {
    world_unit_t* unit;
    s16 job_id;
    s32 index;
    s32 palette;
    s16 clut_x;

    unit = g_world_unit_pointers[formation_index];
    job_id = unit->identity.job_id;
    if (job_id == JOB_ID_BYBLOS) {
        index = 0x4E;
    } else if (job_id == JOB_ID_STEEL_GIANT) {
        index = 0x52;
    } else if (job_id == JOB_ID_APANDA) {
        index = 0x4E;
    } else if (job_id == JOB_ID_SERPENTARIUS) {
        index = 0x4F;
    } else if (job_id == JOB_ID_ARCHAIC_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_ULTIMA_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_HOLY_DRAGON_REIS) {
        index = 0x4C;
    } else if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
        index = unit->monster_graphic + 0x3D;
    } else if (unit->sprite_set < CHARACTER_IDENTITY_BOY) {
        index = g_world_formation_generic_human_graphic_index_table[unit->sprite_set];
    } else {
        world_formation_get_generic_human_graphic_entry(job_id, unit->gender_flags & UNIT_FLAG_FEMALE, destination);
        return;
    }
    bcopy(&g_world_formation_graphic_entries[index], destination, sizeof(*destination));
    palette = (s16)world_formation_get_unit_palette_index(g_world_unit_pointers[formation_index]->identity.roster_slot);
    clut_x = (palette % 4) * 0x10 + 0x140;
    job_id = palette / 4;
    destination->clut = GetClut(clut_x, job_id + 0xE0);
}

/* Find the current formation-list index for a roster slot.
 *
 * Narrow the requested slot to s16. On a miss, return the number of entries searched.
 */
s32 world_formation_find_index_by_roster_slot(s16 roster_slot) {
    s32 index;

    for (index = 0; index < g_world_formation_record_count; index++) {
        if (g_world_unit_pointers[index]->identity.roster_slot == roster_slot) {
            break;
        }
    }
    return index;
}

/* Copy the formation graphic entry of a generic human job. Jobs below 0x35
 * are looked up in a table; of the rest, Bard and Dancer have fixed entries
 * (0x3a, 0x3b), while Mime uses 0x3c and the others job * 2 - 0x7c, both
 * plus one for a female unit. */
void world_formation_get_generic_human_graphic_entry(s32 job_id, s16 is_female, texture_source_t* destination) {
    s16 id = job_id;
    s16 idx;

    if (id < JOB_ID_CHEMIST_35) {
        idx = g_world_formation_generic_human_graphic_index_table[id];
    } else if (id == JOB_ID_DANCER) {
        idx = 0x3B;
    } else if (id == JOB_ID_BARD) {
        idx = 0x3A;
    } else {
        if (id == JOB_ID_MIME) {
            idx = 0x3C;
        } else {
            idx = (job_id * 2) - 0x7C;
        }
        if (is_female != 0) {
            idx += 1;
        }
    }
    bcopy(&g_world_formation_graphic_entries[idx], destination, 0xC);
}

/*
 * Copy a formation unit into the shared temporary record.
 *
 * The snapshot caller supplies the preview index as a second argument; this
 * routine ignores it and always uses g_world_formation_temp_unit.
 */
void world_formation_copy_unit_to_temp(s16 index, s32 unused_slot) {
    bcopy(g_world_unit_pointers[index], g_world_formation_temp_unit, sizeof(world_unit_t));
}
