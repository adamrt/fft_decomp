/* Find resident unit records by id or occupied tile and maintain their iteration order. */

#include "fft/map_unit.h"
#include "fft/sprite_sheet.h"
#include "fft/unit.h"
#include "psx/types.h"

/* The map unit record of the casting unit (g_unit_casting_map_unit_id), or 0. It is the source
 * unit except while a reaction or Hamedo counter resolves, or while a rider's mount takes its
 * move. */
map_unit_t* map_unit_get_casting(void) {
    map_unit_t* unit;

    for (unit = g_map_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit->map_unit_id == g_unit_casting_map_unit_id) {
            return unit;
        }
    }
    return 0;
}

/* The map unit record of the source unit, whose turn or queued between-turn event is being resolved
 * (g_unit_source_map_unit_id), or 0. */
map_unit_t* map_unit_get_source(void) {
    map_unit_t* unit = g_map_unit_list_head;
    s32 source_id;

    if (unit != 0) {
        source_id = g_unit_source_map_unit_id;
        for (; unit != 0; unit = unit->previous) {
            if (unit->map_unit_id == source_id) {
                return unit;
            }
        }
    }
    return 0;
}

/* The first record in the map unit list standing on a tile, whatever its state
 * (crystals, treasures and records without battle stats included), or 0. */
map_unit_t* map_unit_find_at_tile(s32 map_x, s32 map_y, s32 layer) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        if (unit->map_x == map_x && unit->map_y == map_y && unit->map_layer == layer) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* Returns the unit the cursor selects on a tile: one with battle stats in an
 * occupied slot that is not a crystal, a treasure or in the air from Jump.
 *
 * Among several, the source unit (g_unit_source_map_unit_id) is preferred,
 * then a rider over its mount, then the first in list order. Returns 0 for none.
 * The candidate array holds 16 entries and is filled without a bound check. */
map_unit_t* map_unit_find_selectable_at_tile(s32 map_x, s32 map_y, s32 layer) {
    map_unit_t* matches[MAP_UNIT_SLOT_COUNT];
    map_unit_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_map_unit_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_layer == layer) && (unit->battle_unit != 0)
            && (unit->battle_unit->active_battle_unit_id != UNIT_ACTIVE_BATTLE_UNIT_ID_NONE)
            && ((unit->status_flags_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE | UNIT_STATUS_JUMP)) == 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        if (count != 1) {
            for (index = 0; index < count; index++) {
                if (matches[index]->map_unit_id == g_unit_source_map_unit_id) {
                    return matches[index];
                }
            }
            for (index = 0; index < count; index++) {
                if (matches[index]->mount_state == UNIT_MOUNT_STATE_RIDER) {
                    return matches[index];
                }
            }
        }
        return matches[0];
    }
    return 0;
}

/* The crystal or treasure lying on a tile, or 0. Among several, the source unit
 * (g_unit_source_map_unit_id) is preferred, then the first in list order. The
 * candidate array holds 16 entries and is filled without a bound check. */
map_unit_t* map_unit_find_pickup_at_tile(s32 map_x, s32 map_y, s32 layer) {
    map_unit_t* matches[MAP_UNIT_SLOT_COUNT];
    map_unit_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_map_unit_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_layer == layer) && (unit->battle_unit != 0)
            && (unit->battle_unit->active_battle_unit_id != UNIT_ACTIVE_BATTLE_UNIT_ID_NONE)
            && ((unit->status_flags_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE)) != 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            if (matches[index]->map_unit_id == g_unit_source_map_unit_id) {
                return matches[index];
            }
        }
        return matches[0];
    }
    return 0;
}

/* The first unit drawn with the chocobo sprite sheet at (x, y) on the specified layer that is
 * neither the source nor the casting unit, or 0: the chocobo a unit arriving on
 * the tile mounts (map_unit_mount_onto_pending). The candidate array
 * holds 16 entries and is filled without a bound check. */
map_unit_t* map_unit_find_other_chocobo_at_tile(s32 x, s32 y, s32 layer) {
    map_unit_t* unit;
    map_unit_t** cursor;
    s32 count;
    s32 id;
    s32 spritesheet_id;
    map_unit_t* list[MAP_UNIT_SLOT_COUNT];
    map_unit_t* result;
    s32 index;

    unit = g_map_unit_list_head;
    count = 0;
    if (unit != 0) {
        spritesheet_id = SPRITE_SHEET_ID_CHOCOBO;
        cursor = list;
        for (; unit != 0; unit = unit->previous) {
            if (unit->map_x == x && unit->map_y == y && unit->map_layer == layer
                && unit->spritesheet_id == spritesheet_id) {
                *cursor = unit;
                cursor += 1;
                count += 1;
            }
        }
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            result = list[index];
            id = result->map_unit_id;
            if (id != g_unit_source_map_unit_id && id != g_unit_casting_map_unit_id) {
                return result;
            }
        }
    }
    return 0;
}

/* Finds another unit standing at (x, y) on the specified layer: one with battle stats in an
 * occupied slot, other than self and, when self is mounted or ridden, other than
 * its mount partner.
 *
 * Among several, the source unit (g_unit_source_map_unit_id) is preferred,
 * then the first in list order. Returns 0 for none. The candidate array holds 16
 * entries and is filled without a bound check. */
map_unit_t* map_unit_find_overlapping_at_tile(map_unit_t* self, s32 x, s32 y, s32 layer) {
    map_unit_t* list[MAP_UNIT_SLOT_COUNT];
    map_unit_t* unit;
    battle_unit_t* stats;
    s32 count;
    s32 i;

    count = 0;
    for (unit = g_map_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit != self && unit->map_x == x && unit->map_y == y && unit->map_layer == layer) {
            stats = unit->battle_unit;
            if (stats != 0 && stats->active_battle_unit_id != UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
                if (self->mount_state == UNIT_MOUNT_STATE_NONE
                    || unit->map_unit_id != self->mount_partner_map_unit_id) {
                    list[count] = unit;
                    count += 1;
                }
            }
        }
    }
    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (list[i]->map_unit_id == g_unit_source_map_unit_id) {
                return list[i];
            }
        }
        return list[0];
    }
    return 0;
}

/* The runtime (display) record whose map_unit_id matches the requested ID, or 0. */
map_unit_t* map_unit_get(u16 map_unit_id) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        if (unit->map_unit_id == map_unit_id) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* The runtime (display) record whose battle stats occupy unit slot battle_unit_id, or 0. */
map_unit_t* map_unit_get_by_battle_unit_id(u16 battle_unit_id) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        if ((unit->battle_unit != 0) && (unit->battle_unit->battle_unit_id == battle_unit_id)) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* Count the map unit records drawn with the crystal sprite sheet. */
s32 map_unit_count_crystals(void) {
    map_unit_t* unit;
    s32 count = 0;

    unit = g_map_unit_list_head;
    while (unit != 0) {
        if (unit->spritesheet_id == SPRITE_SHEET_ID_CRYSTAL) {
            count++;
        }
        unit = unit->previous;
    }
    return count;
}

/* Moves a unit's record to the head of the map unit list, so list walks reach it
 * first. Returns 1 when moved, 0 when no record has the map unit id. */
s32 map_unit_move_to_list_head(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);
    map_unit_t** link;
    map_unit_t** head;

    if (unit != 0) {
        /* Walk link addresses so the list head needs no fictitious unit record. */
        for (link = &g_map_unit_list_head; link != 0; link = *link != 0 ? &(*link)->previous : 0) {
            if (*link == unit) {
                *link = unit->previous;
                head = &g_map_unit_list_head;
                unit->previous = *head;
                *head = unit;
                return 1;
            }
        }
        return 0;
    }
    return 0;
}
