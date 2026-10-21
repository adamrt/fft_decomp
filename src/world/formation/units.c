/* Formation changes learned abilities and builds the unit and job sprite sources used by its menus. */

#include "fft/ability.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/skillset.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern texture_source_t g_formation_sprite_sources[];

/* Fixed character identities select sprite sources here; jobs below 0x35 use the same mapping. */
extern u8 g_formation_sprite_index_by_identity[];

/*
 * Change an ability slot and optionally replace incompatible equipment.
 *
 * The record is saved to the party and anything the new ability set no
 * longer allows is removed. With `update_equipment`, a unit other than the
 * preview unit and not a Mime is then given its best-fit equipment: the old
 * items return to the inventory before the replacement set is taken from it,
 * and the auto-equip banner (0xD811) shows for 30 frames. Return 1 on success
 * and -2 when the unit is locked (see formation_unit_lock_get).
 */
s32 formation_ability_slot_set(s16 unit_id, s16 slot, s16 ability, s32 update_equipment) {
    s16 equipment[MENU_EQUIPMENT_SLOT_COUNT];
    s32 i;

    if (formation_unit_lock_get(unit_id) == FORMATION_UNIT_UNLOCKED) {
        formation_unit_t* unit = g_formation_unit_pointers[unit_id];
        unit->detail.abilities[slot] = ability;
        formation_save_records();
        formation_invalid_loadout_remove(unit_id);
        if (update_equipment != 0 && unit_id != FORMATION_PREVIEW_UNIT_INDEX
            && g_formation_unit_pointers[unit_id]->identity.job_id != JOB_ID_MIME) {
            formation_item_best_fit_build(unit_id, equipment);
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                item_inventory_count_adjust(g_formation_unit_pointers[unit_id]->detail.equipment[i], 1);
            }
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                g_formation_unit_pointers[unit_id]->detail.equipment[i] = equipment[i];
                item_inventory_count_adjust(equipment[i], -1);
            }
            formation_save_records();
            formation_banner_countdown(0xD811, 0x1E);
        }
        return 1;
    }
    return -2;
}

/* Why a formation unit cannot use a menu, as a FORMATION_UNIT_LOCK_*
 * value: away on a proposition, a monster, an egg or a Mime, tested in that
 * order; FORMATION_UNIT_UNLOCKED otherwise. */
s32 formation_unit_lock_get(s32 unit_id) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_id];

    if (unit->proposition_status != 0)
        return FORMATION_UNIT_LOCK_PROPOSITION;
    if (unit->detail.uses_monster_skillset != 0)
        return FORMATION_UNIT_LOCK_MONSTER;
    if (job_is_special_monster(unit->identity.job_id) != 0)
        return FORMATION_UNIT_LOCK_MONSTER;
    if (g_formation_unit_pointers[unit_id]->gender_flags & UNIT_FLAG_EGG)
        return FORMATION_UNIT_LOCK_EGG;
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_MIME)
        return FORMATION_UNIT_LOCK_MIME;
    return FORMATION_UNIT_UNLOCKED;
}

/* Mark an ability learned for a job: find its position in the job's skillset list and set that
 * bit, most significant first, in the unit's learned-ability bytes for the job. */
void formation_ability_learn(s16 unit_id, s16 job_id, s16 ability_id) {
    s32 skillset = job_skillset_get(job_id);
    s16 generic_job = job_generic_index_get(job_id);
    s16* list = (s16*)ability_skillset_list_build(skillset, SKILLSET_ABILITY_FILTER_ALL);
    s32 i = 0;
    s32 byte;
    s32 bit;
    s32 mask;
    u8* learned_abilities;

    while (list[i] != ability_id) {
        i++;
    }
    byte = i >> 3;
    bit = 7 - (i & 7);
    mask = 1;
    i = mask << bit;
    /* Keeping the job-byte stride before the field address preserves the retail load/add order. */
    learned_abilities = (u8*)g_formation_unit_pointers[unit_id]
        + generic_job * sizeof(g_formation_unit_pointers[unit_id]->learned_abilities[0])
        + offsetof(formation_unit_t, learned_abilities);
    learned_abilities[byte] |= i;
}

/* Build the sprite texture source and palette for a formation unit.
 *
 * Special jobs use fixed sprite entries; generic humans derive theirs from
 * job and gender. The third argument is retained because all known callers
 * pass it even though this function does not consume it. */
void formation_unit_sprite_source_build(s16 formation_index, texture_source_t* destination, s32 unused) {
    formation_unit_t* unit = g_formation_unit_pointers[formation_index];
    s16 job_id;
    s32 index;
    s32 palette;
    s16 clut_x;

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
        index = g_formation_sprite_index_by_identity[unit->sprite_set];
    } else {
        formation_job_sprite_source_build(job_id, unit->gender_flags & UNIT_FLAG_FEMALE, destination);
        return;
    }
    bcopy(&g_formation_sprite_sources[index], destination, sizeof(*destination));
    palette = (s16)formation_unit_palette_index_get(g_formation_unit_pointers[formation_index]->identity.roster_slot);
    clut_x = (palette % 4) * 0x10 + 0x140;
    job_id = palette / 4;
    destination->clut = GetClut(clut_x, job_id + 0xE0);
}

/* Find the current formation-list index for a roster slot.
 *
 * Narrow the requested slot to s16. On a miss, return the number of entries searched.
 */
s32 formation_roster_slot_find(s16 roster_slot) {
    s32 index;

    for (index = 0; index < g_formation_record_count; index++) {
        if (g_formation_unit_pointers[index]->identity.roster_slot == roster_slot) {
            break;
        }
    }
    return index;
}

/* Build the sprite texture source for a job. Jobs below 0x35 share their
 * character identity lookup; of the rest, Bard and Dancer have fixed entries
 * (0x3a, 0x3b), while Mime uses 0x3c and the others job * 2 - 0x7c, both
 * plus one for a female unit. */
void formation_job_sprite_source_build(s32 job_id, s16 is_female, texture_source_t* destination) {
    s16 id = job_id;
    s16 idx;

    if (id < JOB_ID_CHEMIST_35) {
        idx = g_formation_sprite_index_by_identity[id];
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
    bcopy(&g_formation_sprite_sources[idx], destination, sizeof(*destination));
}

/*
 * Copy a formation unit into the shared temporary record.
 *
 * The snapshot caller supplies the preview index as a second argument; this
 * routine ignores it and always uses g_formation_temp_unit.
 */
void formation_unit_to_temp_copy(s16 index, s32 unused_slot) {
    bcopy(g_formation_unit_pointers[index], g_formation_temp_unit, sizeof(formation_unit_t));
}
