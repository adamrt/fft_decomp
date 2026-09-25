#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

/* View of battle_stats_t 0x16e..0x17f (action_actor_id through
 * action_target_y); the target keeps a second pointer biased to 0x16e. */
typedef struct battle_target_reflect_action {
    u8 actor_id;    /* 0x16e */
    u8 skillset_id; /* 0x16f */
    s16 ability_id; /* 0x170 */
    u8 pad172[6];
    u8 reaction_id;       /* 0x178 */
    u8 target_id;         /* 0x179 */
    s16 target_x;         /* 0x17a */
    s16 target_elevation; /* 0x17c */
    s16 target_y;         /* 0x17e */
} battle_target_reflect_action_t;

s32 battle_target_apply_reflect(battle_stats_t* unit) {
    battle_target_reflect_action_t* act;
    s32 x;
    s32 y;
    s32 z;
    s32 dx;
    s32 dy;
    s32 last_attack;

    act = (battle_target_reflect_action_t*)&unit->action_actor_id;
    dx = unit->x - g_acting_unit_x;
    dy = unit->position.bits.y - g_acting_unit_y;
    if (dx == 0 && dy == 0) {
        return -1;
    }
    if (battle_map_resolve_rotated_offset_layer(unit, dx, dy, 0, &x, &y, &z) != 0) {
        return -1;
    }
    act->skillset_id = SKILLSET_ID_BLACK_MAGIC;
    last_attack = *(u16*)&unit->action.last_received_attack;
    act->reaction_id = 5;
    act->target_id = 0;
    act->ability_id = last_attack;
    act->target_x = x;
    act->target_y = y;
    act->target_elevation = z;
    return 0;
}
