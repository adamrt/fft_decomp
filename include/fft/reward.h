#ifndef REWARD_H
#define REWARD_H

/* Battle rewards: the EXP, JP, level ups and war funds an action earns and the reports that show them, and the war
 * trophies and bonus money of the battle's units. */

#include "fft/unit.h"
#include "psx/types.h"

/* action rewards */
/* EXP starts at 10 for a success or 20 divided by the KO count for a kill,
 * and ends within 1..99. */
enum { REWARD_EXP_SUCCESS_BASE = 10, REWARD_EXP_KILL_BASE = 20, REWARD_EXP_MAX = 99 };

/* The experience and JP an action earned and the levels it raised, for the result display. */
typedef struct action_reward_display {
    u8 earned_experience;     /* +0x0 */
    u8 earned_jp;             /* +0x1 */
    u8 level_for_display;     /* +0x2; nonzero selects the "Level UP!" display */
    u8 job_level_for_display; /* +0x3 */
} action_reward_display_t;
typedef char action_reward_display_size_must_be_4[(sizeof(action_reward_display_t) == 4) ? 1 : -1];

s32 reward_apply_action(s32 unit_index, action_reward_display_t* display);
s32 reward_apply_result_and_collect(battle_unit_t* unit, action_reward_display_t* rewards);
void reward_apply_exp_change(battle_unit_t* unit, u8 amount);
s32 reward_get_job_level(battle_unit_t* unit, s32 job_id, s32* out_slot);
s32 reward_add_job_jp(s32 unit_index, s32 jp, s32 team, s32 slot);
u8 reward_calculate_move_get_amount(const battle_unit_t* unit);
s32 reward_add_war_funds(battle_unit_t* unit, s32 total, u8 item_id);

/* war trophies and bonus money */
/* War trophies and bonus money of the battle's units, filled by
 * reward_find_war_trophies_and_bonus_money; REQUIRE fills its own copy
 * (event/require/war_trophies.c) with it for the war trophy screen. */
typedef struct reward_war_result {
    u8 item_ids[BATTLE_UNIT_SLOT_COUNT]; /* 0x00 */
    u8 item_count;                       /* 0x15 */
    u8 _padding_16[2];                   /* 0x16; aligns bonus_money */
    s32 bonus_money;                     /* 0x18; sum of bonus-money modifiers * 100 */
    s32 level_sum_money;                 /* 0x1c; g_reward_enemy_level_sum * 100 */
    s32 level_bonus_money;               /* 0x20; (highest enemy level - lowest level) * 100 */
} reward_war_result_t;
typedef char reward_war_result_size_must_be_36[(sizeof(reward_war_result_t) == 36) ? 1 : -1];

/* Sum and highest level of the units of any team but blue, counted as the ENTD units load and as
 * an off-field unit is added; later level changes are not counted. They set the level-sum and
 * level-bonus money. */
extern u16 g_reward_enemy_level_sum;
extern u8 g_reward_highest_enemy_level;

/* Set when a unit that started on the blue team turns Crystal or Dead; it voids the battle's
 * level bonus money. */
extern u8 g_reward_player_unit_fallen;

void reward_count_enemy_level(battle_unit_t* unit);
void reward_find_war_trophies_and_bonus_money(reward_war_result_t* result);

#endif
