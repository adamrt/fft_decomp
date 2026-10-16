#ifndef REWARD_H
#define REWARD_H

/* Battle rewards: the EXP, JP, level ups and war funds an action earns and the reports that show them, and the war
 * trophies and bonus money of the battle's units. */

#include "fft/action.h"
#include "fft/unit.h"
#include "psx/types.h"

/* action rewards */
/* g_reward_report_phase: the reports shown one after another once an action
 * has resolved. The post-movement reports use the first two. */
typedef enum reward_report_phase {
    REWARD_REPORT_LEVEL_UP = 0,
    REWARD_REPORT_JOB_LEVEL_UP = 1,
    REWARD_REPORT_LEARN_ON_HIT = 2,
} reward_report_phase_e;

extern s32 g_reward_report_phase; /* reward_report_phase_e */

void reward_grant(void);
s32 reward_apply_action(s32 unit_index, action_reward_display_t* display);
s32 reward_build_display(unit_stats_t* unit, action_reward_display_t* rewards);
void reward_apply_exp_change(unit_stats_t* unit, u8 amount);
s32 reward_load_job_level(unit_stats_t* unit, s32 job_id, s32* out_slot);
s32 reward_add_job_jp(s32 unit_index, s32 jp, s32 team, s32 slot);
u8 reward_calculate_move_get_amount(const unit_stats_t* unit);
s32 reward_add_war_funds(unit_stats_t* unit, s32 total, u8 item_id);

/* war trophies and bonus money */
/* War trophies and bonus money of the battle's units, filled by
 * reward_find_war_trophies_and_bonus_money; REQUIRE fills
 * g_war_trophy_search_result with it for the war trophy screen. */
typedef struct reward_war_result {
    u8 item_ids[UNIT_SLOT_COUNT]; /* 0x00 */
    u8 item_count;                /* 0x15 */
    u8 _padding_16[2];            /* 0x16; aligns bonus_money */
    s32 bonus_money;              /* 0x18; sum of bonus-money modifiers * 100 */
    s32 level_sum_money;          /* 0x1c; g_reward_enemy_level_sum * 100 */
    s32 level_bonus_money;        /* 0x20; (highest enemy level - lowest level) * 100 */
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

void reward_count_enemy_level(unit_stats_t* unit);
void reward_find_war_trophies_and_bonus_money(reward_war_result_t* result);

#endif
