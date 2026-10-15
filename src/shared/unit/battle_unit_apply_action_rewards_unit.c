#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Apply the acting unit's pending EXP and JP rewards.
 *
 * EXP saturates at 255 before the level-up check. JP is 8 + job level * 2 +
 * level / 4, from the level and job level before this award, or 0 when the
 * action earned no EXP; monsters earn none. Gained JP Up (* 3/2) affects only
 * the actor; other battle slots receive one quarter of the unmodified JP
 * through the callee's team filter. */
s32 battle_unit_apply_action_rewards(s32 unit_index, action_reward_display_t* display) {
    s32 job_slot;
    unit_stats_t* unit;
    u8 old_level;
    s32 experience;
    s32 jp;
    s32 unit_id;
    u8 team;
    u8 job_id;
    s32 old_job_level;
    s32 new_job_level;
    s32 job_component;
    s32 level_component;

    display->earned_experience = 0;
    display->earned_jp = 0;
    display->level_for_display = 0;
    display->job_level_for_display = 0;

    if (g_action_hamedo_flag != 0) {
        return -1;
    }
    if (g_action_current_ability.can_earn_exp_jp == 0) {
        return 0;
    }

    unit = &g_battle_unit_stats[unit_index];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return -1;
    }

    experience = unit->experience + g_action_current_ability.earned_experience;
    old_level = unit->level;
    if (experience >= 0x100) {
        experience = 0xff;
    }
    unit->experience = experience;
    if (main_unit_check_level_up(unit) != 0 || g_action_current_ability.level_gained_flag != 0) {
        display->level_for_display = unit->level;
    }
    display->earned_experience = g_action_current_ability.earned_experience;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }

    job_id = unit->job_id;
    old_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    job_component = old_job_level * 2;
    level_component = (old_level >> 2) + 8;
    jp = job_component + level_component;
    if (g_action_current_ability.earned_experience == 0) {
        jp = 0;
    }

    team = unit->team_flags & BATTLE_TEAM_MASK;
    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        if (unit_id != unit_index) {
            battle_unit_store_jp_and_calculate_unlocked_jobs(unit_id, jp / 4, team, job_slot);
        }
    }

    if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_JP_UP) {
        jp = jp * 3 / 2;
    }
    team = unit->initial_team_flags & BATTLE_TEAM_MASK;
    battle_unit_store_jp_and_calculate_unlocked_jobs(unit_index, jp, team, job_slot);

    new_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    if (new_job_level != old_job_level) {
        display->job_level_for_display = new_job_level;
    }
    display->earned_jp = jp;
    return 0;
}

/*
 * Returns the unit's level (0..8) in the generic job job_id, from the
 * nibble-packed unit_stats_t.career.job_levels bytes at 0xd2.
 * *out_slot receives the zero-based job slot (Chemist = 1 ... Mime = 19),
 * or 0 for Squire and non-generic jobs.
 */
s32 battle_unit_load_job_level(unit_stats_t* unit, s32 job_id, s32* out_slot) {
    s32 slot;
    s32 packed;
    if (job_id >= JOB_ID_CHEMIST && job_id <= JOB_ID_MIME) {
        *out_slot = job_id - JOB_ID_SQUIRE;
    } else {
        *out_slot = 0;
    }
    slot = *out_slot;
    packed = unit->career.job_levels[slot / 2];
    /* Reassigning packed in both arms keeps the target's `sra`; returning
     * `packed >> 4` directly lets GCC use the byte's range and emit `srl`. */
    if (!(slot & 1)) {
        packed >>= 4;
    } else {
        packed &= 0xF;
    }
    return packed;
}

/* Adds JP to a battle unit's job slot and recomputes its level and unlocks.
 *
 * Skips absent, crystal, dead, treasure, monster and other-team units, and
 * the Bard slot for females and the Dancer slot for males. The battle twin of
 * main_job_add_proposition_jp; returns -1 when nothing is stored. */
s32 battle_unit_store_jp_and_calculate_unlocked_jobs(s32 unit_index, s32 jp, s32 team, s32 slot) {
    unit_stats_t* unit;
    s32 total;
    s32 level;
    s32 levels;
    u8 flags;
    u8 level_byte;

    if (jp < 0) {
        return -1;
    }
    unit = &g_battle_unit_stats[unit_index];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        return -1;
    }
    flags = unit->unit_flags;
    if (flags & UNIT_FLAG_MONSTER) {
        return -1;
    }
    if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != (team & 0xff)) {
        return -1;
    }
    if (slot == UNIT_CAREER_JOB_INDEX_BARD && (flags & UNIT_FLAG_FEMALE)) {
        return -1;
    }
    if (slot == UNIT_CAREER_JOB_INDEX_DANCER && (flags & UNIT_FLAG_MALE)) {
        return -1;
    }
    total = unit->career.job_points.current.values[slot] + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    unit->career.job_points.current.values[slot] = total;
    total = unit->career.job_points.total.values[slot] + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    unit->career.job_points.total.values[slot] = total;
    level = main_job_calculate_level(total);
    levels = unit->career.job_levels[slot / 2];
    level_byte = level;
    if (slot & 1) {
        levels = level + (levels & 0xf0);
    } else {
        levels = (levels & 0xf) | (level_byte << 4);
    }
    unit->career.job_levels[slot / 2] = levels;
    main_job_store_unlock_bitset(
        unit->career.unlocked_jobs, main_job_calculate_unlocked(unit->career.job_levels, unit->unit_flags));
    return 0;
}

/* Find a free tile near a unit for it to be moved to, returning 0 or -1.
 *
 * Panels holding another unit or an earlier destination are marked in
 * unit_record_index. Rings of increasing Manhattan radius around the unit are then
 * searched, first up to radius 3 within -10..+4 of the unit's height, then up
 * to radius 16 at any height, for a flat enough, unblocked tile that is not
 * lava or an obstacle. The chosen tile is also appended to the destination
 * list.
 *
 * The panel clear walks the byte offset itself: the target loop has no index
 * register, which an index loop keeps. Only level's destination store is an
 * assignment chain; that shape alone gives the target's register choices. */
s32 battle_unit_find_relocation_tile(s32 unit_index, map_tile_coordinates_t* destination) {
    unit_stats_t* unit;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    s32 origin_x;
    s32 origin_y;
    s32 low;
    s32 high;
    s32 i;
    s32 n;
    s32 height;
    s32 pass;
    s32 limit;
    s32 dx;
    s32 dy;
    s32 x;
    s32 y;
    s32 side;
    s32 level;
    s32 index;
    s32 surface;
    u32 depth;
    map_tile_flags_t flags;
    s32 found_x;
    s32 found_y;
    s32 total;
    target_panel_t* panel;

    unit = &g_battle_unit_stats[unit_index];
    origin_x = unit->x;
    origin_y = unit->position.y;
    for (n = 0x1ff * sizeof(target_panel_t); n >= 0; n -= sizeof(target_panel_t)) {
        ((target_panel_t*)((u8*)g_target_panels + n))->unit_record_index = 0;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            index = map_calculate_location(unit);
            panel = &g_target_panels[index];
            tile = &g_map_tile_data[index];
            if (i == unit_index) {
                height = tile->height * 2 + tile->depth_half_height.half_height;
                low = height - 10;
                if (low < 0) {
                    low = 0;
                }
                high = height + 4;
            } else {
                panel->unit_record_index = 1;
            }
        }
    }
    for (i = 0; i < g_battle_relocated_unit_count; i++) {
        index = g_battle_relocated_unit_coords[i].elevation * 256 + g_battle_relocated_unit_coords[i].y * g_map_max_x
            + g_battle_relocated_unit_coords[i].x;
        panel = &g_target_panels[index];
        panel->unit_record_index = 1;
    }
    for (pass = 0; pass < 2; pass++) {
        limit = 4;
        if (pass != 0) {
            limit = 0x11;
        }
        for (i = 1; i < limit; i++) {
            for (dx = -i; dx <= i; dx++) {
                x = origin_x + dx;
                if (x < 0 || x >= g_map_max_x) {
                    continue;
                }
                if (dx > 0) {
                    dy = i - dx;
                } else {
                    dy = i + dx;
                }
                side = -1;
                found_x = x;
                for (y = origin_y - dy; side < 2; side += 2) {
                    if (y >= 0 && y < g_map_max_y) {
                        found_y = y;
                        for (level = 0; level < 2; level++) {
                            index = level * 256 + g_map_max_x * y + x;
                            tile = &g_map_tile_data[index];
                            depth = (*(u8*)&tile->depth_half_height);
                            if (tile->depth_half_height.depth >= 3) {
                                continue;
                            }
                            height = depth & MAP_TILE_HALF_HEIGHT_MASK;
                            if (height >= 3) {
                                continue;
                            }
                            flags = tile->flags;
                            if (flags.blocked) {
                                continue;
                            }
                            if (flags.untargetable) {
                                continue;
                            }
                            if (g_target_panels[index].unit_record_index != 0) {
                                continue;
                            }
                            surface = tile->surface.type;
                            if (surface == MAP_SURFACE_LAVA || surface == MAP_SURFACE_OBSTACLE) {
                                continue;
                            }
                            if (pass == 0) {
                                total = tile->height * 2 + height;
                                if (total > high || total < low) {
                                    continue;
                                }
                            }
                            destination->x = found_x;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].x = found_x;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].y = destination->y = found_y;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].elevation = destination->layer
                                = level;
                            g_battle_relocated_unit_count++;
                            return 0;
                        }
                    }
                    y += dy * 2;
                }
            }
        }
    }
    return -1;
}
