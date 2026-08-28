#include "fft/data.h"
#include "fft/job.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * Drop equipment and abilities a formation unit can no longer use.
 *
 * Unequippable items (status 0, -1 or -6) and a second hand weapon without
 * Two Swords are removed, returning items to inventory except for slot 20.
 * Mime clears every ability slot, jobs with an innate support ability drop
 * the redundant copy, and a secondary skillset equal to the primary is
 * cleared. The loop counter and status locals are reused for the two hand
 * items, which keeps the original register assignment.
 */
void world_formation_remove_invalid_unit_loadout(s16 unit_id) {
    s32 i;
    s32 status;

    for (i = 0; i < 5; i++) {
        status = world_formation_can_equip_item_in_slot(
            unit_id, i, (s16)g_world_formation_unit_pointers[unit_id]->equipment[i]);
        if ((u32)(status + 1) < 2 || status == -6) {
            if (unit_id != 20) {
                world_item_change_quantity_on_equip((s16)g_world_formation_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_world_formation_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    i = g_world_formation_unit_pointers[unit_id]->equipment[0];
    status = g_world_formation_unit_pointers[unit_id]->equipment[1];
    if (i < ITEM_ID_THROWABLE_FIRST && i != ITEM_ID_NOTHING && status < ITEM_ID_THROWABLE_FIRST
        && status != ITEM_ID_NOTHING && world_ability_has_two_swords(unit_id) == 0) {
        if (unit_id != 20) {
            world_item_change_quantity_on_equip((s16)g_world_formation_unit_pointers[unit_id]->equipment[1], 1);
        }
        g_world_formation_unit_pointers[unit_id]->equipment[1] = ITEM_ID_NOTHING;
    }
    if (g_world_formation_unit_pointers[unit_id]->job_id == JOB_ID_MIME) {
        memset(&g_world_formation_unit_pointers[unit_id]->primary_skillset, 0, 10);
    }
    if (g_world_formation_unit_pointers[unit_id]->job_id == JOB_ID_CHEMIST
        && (u16)g_world_formation_unit_pointers[unit_id]->support_ability == ABILITY_ID_SUPPORT_THROW_ITEM) {
        g_world_formation_unit_pointers[unit_id]->support_ability = 0;
    }
    if (g_world_formation_unit_pointers[unit_id]->job_id == JOB_ID_MONK
        && (u16)g_world_formation_unit_pointers[unit_id]->support_ability == ABILITY_ID_SUPPORT_MARTIAL_ARTS) {
        g_world_formation_unit_pointers[unit_id]->support_ability = 0;
    }
    if (g_world_formation_unit_pointers[unit_id]->job_id == JOB_ID_MEDIATOR
        && (u16)g_world_formation_unit_pointers[unit_id]->support_ability == ABILITY_ID_SUPPORT_MONSTER_TALK) {
        g_world_formation_unit_pointers[unit_id]->support_ability = 0;
    }
    if (g_world_formation_unit_pointers[unit_id]->job_id == JOB_ID_NINJA
        && (u16)g_world_formation_unit_pointers[unit_id]->support_ability == ABILITY_ID_SUPPORT_TWO_SWORDS) {
        g_world_formation_unit_pointers[unit_id]->support_ability = 0;
    }
    if ((u16)g_world_formation_unit_pointers[unit_id]->primary_skillset
        == (u16)g_world_formation_unit_pointers[unit_id]->secondary_skillset) {
        g_world_formation_unit_pointers[unit_id]->secondary_skillset = 0;
    }
    if (unit_id != 20) {
        world_formation_save_records_to_party_data();
    }
}
