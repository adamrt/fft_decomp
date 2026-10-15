#include "fft/world.h"
#include "psx/types.h"

/*
 * Drop equipment and abilities a formation unit can no longer use.
 *
 * Unequippable items (status 0, -1 or -6) and a second hand weapon without
 * Two Swords are removed, returning items to inventory except for the preview
 * record.
 * Mime clears every ability slot, jobs with an innate support ability drop
 * the redundant copy, and a secondary skillset equal to the primary is
 * cleared. The loop counter and status locals are reused for the two hand
 * items, which keeps the original register assignment.
 */
void world_formation_remove_invalid_unit_loadout(s16 unit_id) {
    s32 i;
    s32 status;

    for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
        status = world_formation_can_equip_item_in_slot(unit_id, i, g_world_unit_pointers[unit_id]->equipment[i]);
        if (status == -1 || status == 0 || status == -6) {
            if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
                item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_world_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    i = g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
    status = g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];
    if (i < ITEM_ID_THROWABLE_FIRST && i != ITEM_ID_NOTHING && status < ITEM_ID_THROWABLE_FIRST
        && status != ITEM_ID_NOTHING && world_ability_has_two_swords(unit_id) == 0) {
        if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
            item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND], 1);
        }
        g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_MIME) {
        memset(&g_world_unit_pointers[unit_id]->abilities, 0, sizeof(world_formation_abilities_t));
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_CHEMIST
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_THROW_ITEM) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_MONK
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_MARTIAL_ARTS) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_MEDIATOR
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_MONSTER_TALK) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_NINJA
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_TWO_SWORDS) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->abilities.fields.primary_skillset
        == g_world_unit_pointers[unit_id]->abilities.fields.secondary_skillset) {
        g_world_unit_pointers[unit_id]->abilities.fields.secondary_skillset = 0;
    }
    if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
        world_formation_save_records_to_party();
    }
}
