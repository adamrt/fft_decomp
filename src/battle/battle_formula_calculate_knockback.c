#include "fft/battle.h"
#include "fft/battle_move.h"
#include "fft/map.h"
#include "fft/status.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Picks the knockback destination for the current action's target.
 *
 * Runs only while g_current_ability.target_count is 1. An immortal target, a target
 * with both low team-flag bits set, the attacker itself and a mounted pair are
 * never pushed. The target moves one tile away from the attacker along the
 * dominant axis, and the tile must be neither untargetable, deep, steep, lava
 * nor an obstacle. On success g_current_ability.knockback_flags records the knockback
 * kind (0x82 for a flier that is neither frog nor chicken, else 0x81),
 * [0xa]..[0xc] the destination x, y and level, and the target's action data
 * gets the knockback special effect.
 *
 * abs() is the builtin: its MIPS abssi2 pattern emits the target's
 * bgez/move/negu sequence. The coordinate stores are array elements so their
 * aggregate aliasing keeps the special-effect load after them. */
void battle_formula_calculate_knockback(void) {
    battle_stats_t* target;
    battle_stats_t* attacker;
    map_tile_t* tile;
    map_tile_depth_t* depth;
    u8 surface;
    s32 dx;
    s32 dy;
    s32 adx;
    s32 ady;
    s32 direction;
    s32 x;
    s32 y;
    s32 level;

    if (g_current_ability.target_count != 1) {
        return;
    }
    target = g_battle_action_target;
    if (target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL) {
        return;
    }
    if ((target->team_flags & 3) == 3) {
        return;
    }
    attacker = g_battle_action_attacker;
    if (target == attacker) {
        return;
    }
    if (target->mount_info != 0) {
        return;
    }
    dx = target->x - attacker->x;
    dy = target->position.bits.y - attacker->position.bits.y;
    adx = abs(dx);
    ady = abs(dy);
    if (adx + ady == 0) {
        return;
    }
    x = target->x;
    y = target->position.bits.y;
    if (ady < adx) {
        if (dx > 0) {
            direction = 3;
            x++;
        } else {
            direction = 1;
            x--;
        }
    } else {
        if (dy > 0) {
            direction = 2;
            y++;
        } else {
            direction = 0;
            y--;
        }
    }
    level = battle_move_check_knockback_destination(direction, x, y);
    if (level < 0) {
        return;
    }
    tile = &g_battle_map_tile_data[(level << 8) + y * g_map_max_x + x];
    surface = tile->surface.bits.type;
    if (tile->flags_06.bits.untargetable) {
        return;
    }
    depth = (map_tile_depth_t*)&tile->depth_half_height;
    if (depth->bits.depth != 0) {
        return;
    }
    if (depth->bits.half_height >= 3) {
        return;
    }
    if (surface == MAP_SURFACE_LAVA || surface == MAP_SURFACE_OBSTACLE) {
        return;
    }
    if (!(battle_move_get_effective_flags(g_battle_action_target) & BATTLE_EFFECTIVE_MOVEMENT_FLY)) {
        g_current_ability.knockback_flags = 0x81;
    } else if (g_battle_action_target->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FROG)]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHICKEN) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))) {
        g_current_ability.knockback_flags = 0x81;
    } else {
        g_current_ability.knockback_flags = 0x82;
    }
    g_current_ability.target_x = x;
    g_current_ability.target_y = y;
    g_current_ability.target_elevation = level;
    g_battle_action_target_data->special_effect |= BATTLE_ACTION_SPECIAL_EFFECT_KNOCKBACK;
}
