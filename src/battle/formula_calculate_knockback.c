#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/* Picks the knockback destination for the current action's target.
 *
 * Runs only while g_action_current_ability.target_count is 1. An immortal target, a target
 * with both low team-flag bits set, the attacker itself and a mounted pair are
 * never pushed. The target moves one tile away from the attacker along the
 * dominant axis, and the tile must be neither untargetable, deep, steep, lava
 * nor an obstacle. On success g_action_current_ability.knockback_flags records the knockback
 * kind (0x82 for a flier that is neither frog nor chicken, else 0x81),
 * [0xa]..[0xc] the destination x, y and level, and the target's action data
 * gets the knockback special effect.
 *
 * abs() is the builtin: its MIPS abssi2 pattern emits the target's
 * bgez/move/negu sequence. The coordinate stores are array elements so their
 * aggregate aliasing keeps the special-effect load after them. */
void formula_calculate_knockback(void) {
    unit_stats_t* target;
    unit_stats_t* attacker;
    map_tile_t* tile;
    u8 surface;
    s32 dx;
    s32 dy;
    s32 adx;
    s32 ady;
    s32 direction;
    s32 x;
    s32 y;
    s32 level;

    if (g_action_current_ability.target_count != 1) {
        return;
    }
    target = g_action_target;
    if (target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL) {
        return;
    }
    if ((target->team_flags & BATTLE_TEAM_IMPORTANCE_MASK) == BATTLE_TEAM_IMPORTANCE_MASK) {
        return;
    }
    attacker = g_action_attacker;
    if (target == attacker) {
        return;
    }
    if (target->mount_info != 0) {
        return;
    }
    dx = target->x - attacker->x;
    dy = target->position.y - attacker->position.y;
    adx = abs(dx);
    ady = abs(dy);
    if (adx + ady == 0) {
        return;
    }
    x = target->x;
    y = target->position.y;
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
    level = move_check_knockback_destination(direction, x, y);
    if (level < 0) {
        return;
    }
    tile = &g_map_tile_data[(level << 8) + y * g_map_max_x + x];
    surface = tile->surface.type;
    if (tile->flags.untargetable) {
        return;
    }
    if (tile->depth_half_height.depth != 0) {
        return;
    }
    if (tile->depth_half_height.half_height >= 3) {
        return;
    }
    if (surface == MAP_SURFACE_LAVA || surface == MAP_SURFACE_OBSTACLE) {
        return;
    }
    if (!(move_get_effective_flags(g_action_target) & BATTLE_EFFECTIVE_MOVEMENT_FLY)) {
        g_action_current_ability.knockback_flags = BATTLE_KNOCKBACK_PENDING | BATTLE_KNOCKBACK_FALL_DAMAGE;
    } else if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        g_action_current_ability.knockback_flags = BATTLE_KNOCKBACK_PENDING | BATTLE_KNOCKBACK_FALL_DAMAGE;
    } else {
        g_action_current_ability.knockback_flags = BATTLE_KNOCKBACK_PENDING | BATTLE_KNOCKBACK_NO_FALL_DAMAGE;
    }
    g_action_current_ability.target_new_x = x;
    g_action_current_ability.target_new_y = y;
    g_action_current_ability.target_new_elevation = level;
    g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_KNOCKBACK;
}
