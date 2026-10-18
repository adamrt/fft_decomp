#ifndef ACTION_H
#define ACTION_H

/* Acting with an ability or attack: preparing it, resolving each strike against its targets, the action result
 * fields, Mimic, effect messages and weapon sounds. */

#include "fft/ability.h"
#include "fft/item.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/types.h"

struct action_data;

/* preparation */
/* Result of action_prepare, kept in unit_t.attack_phase_state. */
typedef enum action_preparation {
    ACTION_PREPARATION_INVALID = -1,
    ACTION_PREPARATION_CHARGE = 0, /* the action has a charge time; also Defend */
    ACTION_PREPARATION_IMMEDIATE = 1,
    ACTION_PREPARATION_CHANGE_EQUIPMENT = 2,
    ACTION_PREPARATION_JUMP = 3
} action_preparation_e;

void action_copy_data(const action_command_t* source, action_command_t* destination);
s32 action_prepare(action_command_t* source, action_command_t* dest, s32 phase);
void action_prepare_in_place(action_command_t* action);
s32 action_commit_preparation(action_command_t* command);
void action_execute_ability(void);

/* action results */
/* Bitset in action_data_t.attack_type; one result can affect HP and MP. */
typedef enum action_type_flags {
    ACTION_TYPE_PSEUDO_STATUS = 0x01,
    ACTION_TYPE_STATUS_CHANGE = 0x08,
    ACTION_TYPE_MP_HEALING = 0x10,
    ACTION_TYPE_MP_DAMAGE = 0x20,
    ACTION_TYPE_HP_HEALING = 0x40,
    ACTION_TYPE_HP_DAMAGE = 0x80
} action_type_flags_e;

typedef enum action_miss_type {
    ACTION_MISS_TYPE_HIT = 0,
    ACTION_MISS_TYPE_ACCESSORY_EVADE = 1,
    ACTION_MISS_TYPE_RIGHT_HAND_EVADE = 2,
    ACTION_MISS_TYPE_LEFT_HAND_EVADE = 3,
    ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD = 4,
    ACTION_MISS_TYPE_NULLIFIED = 5,
    ACTION_MISS_TYPE_ACCURACY_MISS = 6,
    ACTION_MISS_TYPE_FORCED_FAILURE = 7,
    ACTION_MISS_TYPE_CANCELED = 8,
    ACTION_MISS_TYPE_REFLECTED = 9,
    ACTION_MISS_TYPE_GOLEM_GUARD = 10,
    ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD = 11,
    ACTION_MISS_TYPE_BUGGED_REFLECTION = 12,
    ACTION_MISS_TYPE_CATCH = 13
} action_miss_type_e;

typedef enum action_special_effect {
    ACTION_SPECIAL_EFFECT_SET_GOLEM = 0x0001,
    ACTION_SPECIAL_EFFECT_MORBOL = 0x0002,
    ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT = 0x0004,
    ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN = 0x0008,
    ACTION_SPECIAL_EFFECT_STEAL_ITEM = 0x0010,
    ACTION_SPECIAL_EFFECT_POACH = 0x0020,
    ACTION_SPECIAL_EFFECT_TEAM_CHANGE = 0x0040,
    ACTION_SPECIAL_EFFECT_LEVEL_UP = 0x0080,
    ACTION_SPECIAL_EFFECT_LEVEL_DOWN = 0x0100,
    ACTION_SPECIAL_EFFECT_ADDED_EFFECT = 0x0200,
    ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION = 0x0400,
    ACTION_SPECIAL_EFFECT_ELEMENTAL_WEAKNESS = 0x0800,
    ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN = 0x1000,
    ACTION_SPECIAL_EFFECT_KNOCKBACK = 0x4000,
    ACTION_SPECIAL_EFFECT_GOLEM_GUARD = 0x8000
} action_special_effect_e;

/* Bit order mirrors the seven serialized unit_stats_t equipment slots. */
typedef enum action_equipment_slot_mask {
    ACTION_EQUIPMENT_SLOT_HEAD = 0x80,
    ACTION_EQUIPMENT_SLOT_BODY = 0x40,
    ACTION_EQUIPMENT_SLOT_ACCESSORY = 0x20,
    ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON = 0x10,
    ACTION_EQUIPMENT_SLOT_RIGHT_SHIELD = 0x08,
    ACTION_EQUIPMENT_SLOT_LEFT_WEAPON = 0x04,
    ACTION_EQUIPMENT_SLOT_LEFT_SHIELD = 0x02,
    /* Every slot bit: stored by the Steal/Break slot picker before it chooses one. */
    ACTION_EQUIPMENT_SLOT_ALL = 0xfe
} action_equipment_slot_mask_e;

/* Ordinary SP/PA/MA/Brave/Faith action-result encoding. CT and EXP reuse
 * these bits with distinct sentinel or sign meanings and are excluded. */
enum { ACTION_STAT_CHANGE_VALUE_MASK = 0x7f, ACTION_STAT_CHANGE_INCREASE = 0x80 };

/* action_data_t.exp_change: the low seven bits are the amount, and the top bit
 * marks a loss, the opposite sense of ACTION_STAT_CHANGE_INCREASE. */
enum { ACTION_EXP_CHANGE_VALUE_MASK = 0x7f, ACTION_EXP_CHANGE_LOSS = 0x80 };

/* HP and MP damage and healing stop at 999. */
enum { ACTION_VALUE_MAX = 999 };

void action_clear_current_data(action_data_t* action);
void action_finalize_target_result(void);
s32 action_apply_target_result(s32 battle_id);
s32 action_apply_attacker_result(s32 unit_id);

/* strikes */
/* Primary strikes versus secondary applications, including movement benefits.
 * Execution, AI simulation and preview are the separate action_state_e axis. */
typedef enum action_context { ACTION_CONTEXT_PRIMARY = 0, ACTION_CONTEXT_SECONDARY = 1 } action_context_e;

typedef enum action_state {
    ACTION_STATE_EXECUTE = 0,
    ACTION_STATE_AI_SIMULATION = 1,
    ACTION_STATE_PREVIEW = 2
} action_state_e;

/* Why action_init_current_data rejects an action; result_queue_charge_failure_message
 * shows a pop-up for the codes below 4. */
typedef enum action_rejection {
    ACTION_REJECTION_NONE = 0,
    ACTION_REJECTION_NO_TARGET = 1, /* pop-up mode only; never returned */
    ACTION_REJECTION_SILENCED = 2,
    ACTION_REJECTION_NOT_ENOUGH_MP = 3,
    ACTION_REJECTION_FROG = 5,   /* a Frog using Charge or an ability other than Attack, Frog Attack or Frog */
    ACTION_REJECTION_HAMEDO = 6, /* a Hamedo reaction pre-empted the action */
    /* A unit_action_restriction_e plus this offset. */
    ACTION_REJECTION_RESTRICTION_OFFSET = 6,
    ACTION_REJECTION_CANNOT_ACT = 7,
    ACTION_REJECTION_DEEP_WATER = 8
} action_rejection_e;

/* g_action_phase: which unit's action the attack phase is announcing. */
typedef enum action_phase {
    ACTION_PHASE_PRE_ACTION = 0,   /* a reaction that pre-empts the pending action (Hamedo) */
    ACTION_PHASE_ACTING_UNIT = 1,  /* the acting unit's own action */
    ACTION_PHASE_REACTING_UNIT = 2 /* the reactions it provoked */
} action_phase_e;

enum { ACTION_PHASE_COUNT = 3 };

/* g_action_current_ability: the attacker/target pair, weapons, formula
 * inputs and results of the strike being resolved, with copies of the ability's
 * secondary data, the weapon's data and the status-infliction record that
 * action_calculate_target_result loads. */
typedef struct action_current_ability {
    u8 attacker_id;                             /* 0x00 */
    u8 target_id;                               /* 0x01 */
    u8 strike_count;                            /* 0x02 */
    u8 strike_counter;                          /* 0x03: weapon hand, 0 = right, 1 = left */
    u8 primary_weapon_id;                       /* 0x04 */
    u8 secondary_weapon_id;                     /* 0x05 */
    u16 reaction_id;                            /* 0x06 */
    u16 weapon_proc_pending;                    /* 0x08; a weapon proc spell is still to strike */
    u8 target_new_x;                            /* 0x0a; knockback or random-fire tile */
    u8 target_new_y;                            /* 0x0b */
    u8 target_new_elevation;                    /* 0x0c */
    u8 terrain;                                 /* 0x0d: surface type under the target */
    s16 xa;                                     /* 0x0e */
    s16 ya;                                     /* 0x10 */
    u8 target_faith;                            /* 0x12 */
    u8 attacker_faith;                          /* 0x13 */
    u8 skillset;                                /* 0x14 */
    u8 _padding_15;                             /* 0x15; aligns ability_id */
    u16 ability_id;                             /* 0x16 */
    u8 weapon_id;                               /* 0x18 */
    u8 two_hands_active;                        /* 0x19 */
    u8 proc_id;                                 /* 0x1a */
    u8 used_item_id;                            /* 0x1b */
    u8 base_hit;                                /* 0x1c */
    u8 _unused_1d;                              /* 0x1d */
    u8 accessory_evade;                         /* 0x1e */
    u8 right_hand_evade;                        /* 0x1f; shield, or weapon with Weapon Guard */
    u8 left_hand_evade;                         /* 0x20 */
    u8 class_evade;                             /* 0x21 */
    u8 facing_modifier;                         /* 0x22: 0 front, 1 side, 2 back */
    u8 _unused_23;                              /* 0x23 */
    u8 charge_power;                            /* 0x24 */
    u8 formula;                                 /* 0x25 */
    u8 target_is_undead;                        /* 0x26 */
    u8 earned_experience;                       /* 0x27 */
    u8 can_earn_exp_jp;                         /* 0x28 */
    u8 defaulted_to_attack;                     /* 0x29 */
    u8 random_damage_factor;                    /* 0x2a */
    u8 mp_cost;                                 /* 0x2b: effective cost recorded before the MP checks of
                                                 * action_check_and_consume_mp; reaction
                                                 * eligibility reads it even when no MP was subtracted */
    u8 target_count;                            /* 0x2c */
    u8 knockback_flags;                         /* 0x2d */
    u8 knockback_fall_height;                   /* 0x2e */
    u8 post_action_target_id;                   /* 0x2f */
    ability_secondary_data_t range_data;        /* 0x30 */
    weapon_data_t weapon_data;                  /* 0x3e */
    status_infliction_data_t status_infliction; /* 0x46 */
    u8 level_gained_flag;                       /* 0x4c */
    u8 job_level_gained_flag;                   /* 0x4d */
    u8 post_action_message;                     /* 0x4e; 6 poached, 9 trained */
    u8 random_fire_flag;                        /* 0x4f */
} action_current_ability_t;
typedef char action_current_ability_size_must_be_80[(sizeof(action_current_ability_t) == 80) ? 1 : -1];

/* action_current_ability_t.post_action_message, copied to the strike work and
 * the runtime unit record: the message the post-action state opens. */
typedef enum action_post_message {
    ACTION_POST_MESSAGE_NONE = 0,
    ACTION_POST_MESSAGE_POACHED = 6,
    ACTION_POST_MESSAGE_TRAINED = 9
} action_post_message_e;

/* The experience and JP an action earned and the levels it raised, for the result display. */
typedef struct action_reward_display {
    u8 earned_experience;     /* +0x0 */
    u8 earned_jp;             /* +0x1 */
    u8 level_for_display;     /* +0x2; nonzero selects the "Level UP!" display */
    u8 job_level_for_display; /* +0x3 */
} action_reward_display_t;
typedef char action_reward_display_size_must_be_4[(sizeof(action_reward_display_t) == 4) ? 1 : -1];

/* The strike work record, embedded in unit_t.strike (+0x18c)
 * and used as a standalone AI work record. Reaction targeting writes its
 * redirected tile at +0x1c..+0x1e before the strike is processed. */
typedef struct action_strike_work {
    u8 actor_id;
    u8 target_count;
    u8 target_list[UNIT_RUNTIME_SLOT_COUNT];
    u8 skip_empty_tile_effect;
    u8 math_skill_flag;
    s16 last_attack_id; /* 0x14 */
    u8 post_action_message;
    u8 reaction_occurred;
    u8 continue_attack;
    u8 current_hit_number;
    s16 reaction_id;
    u8 target_new_x;
    u8 target_new_y;
    u8 target_new_map_level;
    u8 used_weapon_id;
    s16 reaction_ability_id; /* 0x20; runtime 0x1ac */
    /* 0x22; runtime 0x1ae. g_action_current_ability.knockback_flags & 0x7f, stored with
     * the knockback destination. */
    u8 knockback_flags;
    u8 can_earn_experience;
    action_reward_display_t action_rewards; /* 0x24; runtime 0x1b0..0x1b3 */
} action_strike_work_t;
typedef char action_strike_work_size_must_be_40[sizeof(action_strike_work_t) == 40 ? 1 : -1];

extern action_state_e g_action_state;
extern action_context_e g_action_context;
extern s32 g_action_phase;
extern action_current_ability_t g_action_current_ability;
extern u8 g_action_loaded_ability_flags_1;
extern unit_stats_t* g_action_attacker;
extern action_data_t* g_action_attacker_data;
extern action_data_t g_action_attacker_action_data;
extern unit_stats_t* g_action_target;
extern action_data_t* g_action_target_data;
extern u8 g_action_canceled_statuses[];
extern action_command_t g_action_saved_command;
extern s32 g_action_actor_id;
extern u8 g_action_actor_x;
extern u8 g_action_actor_y;
extern u8 g_action_actor_elevation;
extern s16 g_action_actor_used_ability_id;

/* Golem barrier HP per team, shared by action resolution and AI snapshots. */
extern u16 g_action_team_golem[UNIT_TEAM_COUNT];

/* Provisional: written only by the uncalled action_store_acting_unit_value; never read. */
extern u8 g_action_acting_unit_value;

s32 action_init_current_data(s32 unit_id);
void action_store_actor_data(unit_stats_t* unit);
s32 action_resolve_strike(s32 battle_id, action_strike_work_t* work);
void action_store_used_weapon(action_command_t* action);
s32 action_calculate_target_result(const action_command_t* source, u8 target_id);
void action_switch_ability_to_default_attack(void);
void action_start_next_strike(void);
void action_apply_strike_results(void);
void action_start_target_response(unit_t* attacker, unit_t* target);
s32 action_start_target_response_by_runtime_id(s32 runtime_id);
void action_start_projectile_hit_response_by_runtime_id(u16 attacker_id, u16 target_id);
void action_store_target_ids(u8* src);
s32 action_copy_ability_targets(u8* out);
void action_remove_ability_target(s32 target_battle_id);
void action_post_action_facing_stub(unit_stats_t* unused_unit, s32 unused_facing);
void action_end_command(unit_t* unit);

/* Preview action calculation entry run on the main stack. */
s32 action_calculate_projected_effect(unit_stats_t* attacker, unit_stats_t* target, action_command_t* command);

/* Like the state_enter_ functions (state.h), sets up a state and switches g_game_state to it, but only when a
 * Hamedo, action or reaction is due. */
s32 action_announce_next(void);

/* mimic */
/* g_action_mimic_attacker, filled by action_mimic_record and read
 * by action_mimic_set_ability. The AI saves and restores it whole as 30 bytes
 * (ai_simulate_begin and ai_simulate_end). */
typedef struct action_mimic_attacker {
    u8 facing;               /* 0x00 */
    u8 mimic_pending;        /* 0x01: a non-Mime action was recorded; state 13 queues the Mimes */
    u8 right_hand_weapon_id; /* 0x02 */
    u8 left_hand_weapon_id;  /* 0x03 */
    u8 team;                 /* 0x04 */
    u8 _padding_05;          /* 0x05; aligns target_delta_x */
    s16 target_delta_x;      /* 0x06 */
    s16 target_delta_y;      /* 0x08 */
    action_command_t action; /* 0x0a: copy of unit_stats_t +0x16e */
} action_mimic_attacker_t;
typedef char action_mimic_attacker_size_must_be_30[(sizeof(action_mimic_attacker_t) == 30) ? 1 : -1];

extern action_mimic_attacker_t g_action_mimic_attacker;

void action_mimic_record(unit_stats_t* unit);
s32 action_mimic_set_ability(unit_stats_t* unit);

/* effect messages */
/* Queue of post-action "effect message" popups (g_action_messages), filled by
 * action_message_queue_for_unit and drained newest first by
 * action_message_show_next. */
typedef struct action_post_effect_msg {
    u8 code;           /* 0x00 action_display_image_id_e of the popup */
    u8 unit;           /* 0x01 battle id of the reported unit */
    u8 _padding_02[2]; /* 0x02; aligns value */
    s32 value;         /* 0x04 reported amount */
} action_post_effect_msg_t;
typedef char action_post_effect_msg_size_must_be_8[(sizeof(action_post_effect_msg_t) == 8) ? 1 : -1];

enum { ACTION_POST_EFFECT_MSG_CAPACITY = 16 };

extern action_post_effect_msg_t g_action_messages[ACTION_POST_EFFECT_MSG_CAPACITY];

void action_message_show_for_caster(void);
s32 action_message_show_next(void);

/* weapon sounds */
void action_play_weapon_sfx(struct unit* unit, s32 hit);

#endif
