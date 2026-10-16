#ifndef BATTLE_ACTION_H
#define BATTLE_ACTION_H

/* Battle actions: the turn order and its active time list, preparing an action, resolving its
 * strikes, reactions and Mimes, and the results and rewards it shows. */

#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/data_ability.h"
#include "fft/data_item.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_at_list.h"
#include "fft/unit.h"
#include "psx/types.h"

/* turns */
/* turn_run_clock modes. */
typedef enum turn_clock_mode {
    TURN_CLOCK_MODE_RUN = 0,
    TURN_CLOCK_MODE_RESET = 1,
    TURN_CLOCK_MODE_FIND_ACTIVE_UNIT = 2,
} turn_clock_mode_e;

/* g_turn_clock_state and g_turn_clock_resume_state: the phases of
 * turn_run_clock. */
typedef enum turn_clock_state {
    TURN_CLOCK_STATE_ADD_CT = 0,         /* every unit gains CT by its speed */
    TURN_CLOCK_STATE_PICK_UNIT = 1,      /* the highest CT of 100 or more takes a turn */
    TURN_CLOCK_STATE_TICK_CHARGES = 2,   /* count down every charged_ability_ct */
    TURN_CLOCK_STATE_RESOLVE_CHARGE = 3, /* a charge that reached 0 resolves */
    TURN_CLOCK_STATE_EXPIRE_STATUSES = 4,
    TURN_CLOCK_STATE_COLLECT_PENDING_ACTIONS = 5, /* note the units whose action has an attack type */
    TURN_CLOCK_STATE_RUN_PENDING_ACTIONS = 6,
    TURN_CLOCK_STATE_PICK_UNIT_AFTER_CHARGE = 9, /* a unit whose CT is 0xff takes a turn at once */
    TURN_CLOCK_STATE_UNIT_TURN = 10,             /* end the turn of each unit that has one */
    TURN_CLOCK_STATE_MARK_MIMES = 13,
    TURN_CLOCK_STATE_RUN_MIMES = 14,
    TURN_CLOCK_STATE_TERRAIN_POISON = 15,
    TURN_CLOCK_STATE_REMOVE_TRANSPARENT = 17,
    TURN_CLOCK_STATE_POISON_AND_REGEN = 19,
} turn_clock_state_e;

typedef enum turn_status_flags {
    TURN_STATUS_CT_FROZEN = 0x01,
    TURN_STATUS_INCAPACITATED = 0x02,
    TURN_STATUS_DEAD = 0x04,
    TURN_STATUS_SLEEP = 0x08,
    TURN_STATUS_HASTE = 0x10,
    TURN_STATUS_SLOW = 0x20,
    TURN_STATUS_BLOCKS_WAIT_MENU_MASK = 0x0f,
    TURN_STATUS_BLOCKS_TURN_MASK = 0x0d,
} turn_status_flags_e;

typedef enum turn_event {
    TURN_EVENT_UNIT_READY = 0x0100,
    TURN_EVENT_ABILITY_READY = 0x0200,
    TURN_EVENT_ACTION_RESULT = 0x0300,
    TURN_EVENT_UNKNOWN_0400 = 0x0400,
    TURN_EVENT_MIME = 0x0500,
    TURN_EVENT_NONE = 0xff00,
} turn_event_e;

/* End of turn: CT gains 20 for an unused move and 20 for an unused action, and
 * the result is capped at 60. */
enum {
    TURN_UNUSED_CT_REFUND = 20,
    TURN_END_CT_MAX = 60,
};

extern s32 g_turn_event_type;
extern u8 g_action_actor_elevation;
extern s32 g_action_actor_id;
extern u8 g_action_actor_x;
extern u8 g_action_actor_y;
extern s32 g_turn_clock_resume_state;
extern s32 g_turn_clock_state;
extern s32 g_state_wait_direction_facing;
extern s32 g_turn_unit_id;

s32 event_check_battle_outcome(void);
void turn_start_next_event(void);
s32 turn_is_over(s32 unit_id);
void state_enter_wait_direction(void);
void state_enter_wait_menu(void);
s32 turn_end(s32 unit_id);
s32 turn_run_clock(turn_clock_mode_e mode);
void turn_set_act_taken(s32 unit_id);
void turn_set_move_taken(s32 unit_id);

/* active time list */
/* at_list_build modes. */
typedef enum at_list_mode {
    AT_LIST_MODE_ALL = 0,
    AT_LIST_MODE_TURNS_ONLY = 1,   /* leave out the charged and performing actions */
    AT_LIST_MODE_ACTIONS_ONLY = 2, /* leave out the active unit and the simulated turns */
    AT_LIST_MODE_PREVIEW = 3,      /* as ALL, with the acting unit's new charge counted from this tick */
} at_list_mode_e;

/* at_list_get_entry_unit_id results: the low byte is the unit's battle id, and a
 * charged-action entry adds AT_LIST_ENTRY_CHARGED_ACTION. */
enum {
    AT_LIST_ENTRY_EMPTY = -4,
    AT_LIST_ENTRY_BATTLE_ID_MASK = 0xff,
    AT_LIST_ENTRY_CHARGED_ACTION = 0x100,
};

extern s32 g_cursor_visible;
extern s32 g_at_list_cursor;

s32 at_list_build(at_entry_t* list, at_list_mode_e mode);
s32 at_list_get_entry_unit_id(s32 at_index);
s32 at_list_find_unit(const unit_stats_t* unit);
void at_list_reset_cursor(void);
void at_list_show_entry(s32 at_index);
u8 at_list_count_turns_to_resolve(s32 battle_id, s32 ct, const at_entry_t* entries);
void menu_status_open(void);
s32 at_list_preview(unit_stats_t* unit, action_command_t* action, at_entry_t* at_list);
void at_list_show_next_entry(void);

/* preparation */
/* Result of action_prepare, kept in unit_t.attack_phase_state. */
typedef enum action_preparation {
    ACTION_PREPARATION_INVALID = -1,
    ACTION_PREPARATION_CHARGE = 0, /* the action has a charge time; also Defend */
    ACTION_PREPARATION_IMMEDIATE = 1,
    ACTION_PREPARATION_CHANGE_EQUIPMENT = 2,
    ACTION_PREPARATION_JUMP = 3,
} action_preparation_e;

void action_prepare_in_place(action_command_t* action);
s32 action_commit_preparation(action_command_t* command);
void state_enter_confirm_action(void);
s32 ability_get_elemental_id(const unit_stats_t* unit);
void state_enter_unit_move(void);
s32 action_prepare(action_command_t* source, action_command_t* dest, s32 phase);
void state_enter_event_at_battle_start(void);
void action_switch_ability_to_default_attack(void);

/* action outcomes */
/* Bitset in action_data_t.attack_type; one result can affect HP and MP. */
typedef enum action_type_flags {
    ACTION_TYPE_PSEUDO_STATUS = 0x01,
    ACTION_TYPE_STATUS_CHANGE = 0x08,
    ACTION_TYPE_MP_HEALING = 0x10,
    ACTION_TYPE_MP_DAMAGE = 0x20,
    ACTION_TYPE_HP_HEALING = 0x40,
    ACTION_TYPE_HP_DAMAGE = 0x80,
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
    ACTION_MISS_TYPE_CATCH = 13,
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
    ACTION_SPECIAL_EFFECT_GOLEM_GUARD = 0x8000,
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
    ACTION_EQUIPMENT_SLOT_ALL = 0xfe,
} action_equipment_slot_mask_e;

/* strikes */
typedef enum action_context {
    ACTION_CONTEXT_PRIMARY = 0,
    ACTION_CONTEXT_REACTION_OR_SIMULATION = 1,
} action_context_e;

typedef enum action_state {
    ACTION_STATE_EXECUTE = 0,
    ACTION_STATE_AI_SIMULATION = 1,
    ACTION_STATE_PREVIEW = 2,
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
    ACTION_REJECTION_DEEP_WATER = 8,
} action_rejection_e;

/* g_action_phase: which unit's action the attack phase is announcing. */
typedef enum action_phase {
    ACTION_PHASE_PRE_ACTION = 0,    /* a reaction that pre-empts the pending action (Hamedo) */
    ACTION_PHASE_ACTING_UNIT = 1,   /* the acting unit's own action */
    ACTION_PHASE_REACTING_UNIT = 2, /* the reactions it provoked */
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

/* action_current_ability_t.knockback_flags: the kind of knockback found by
 * move_knockback_calculate, with PENDING set until it is applied. */
typedef enum move_knockback_kind {
    MOVE_KNOCKBACK_FALL_DAMAGE = 1,    /* a grounded target: the fall height can hurt */
    MOVE_KNOCKBACK_NO_FALL_DAMAGE = 2, /* a flying target */
} move_knockback_kind_e;

/* Encoding of the kind and pending marker in the same byte. */
enum {
    MOVE_KNOCKBACK_KIND_MASK = 0x7f,
    MOVE_KNOCKBACK_PENDING = 0x80,
};

/* action_current_ability_t.post_action_message, copied to the strike work and
 * the runtime unit record: the message the post-action state opens. */
typedef enum action_post_message {
    ACTION_POST_MESSAGE_NONE = 0,
    ACTION_POST_MESSAGE_POACHED = 6,
    ACTION_POST_MESSAGE_TRAINED = 9,
} action_post_message_e;

extern unit_stats_t* g_action_attacker;
extern action_data_t g_action_attacker_action_data;
extern action_data_t* g_action_attacker_data;
extern u8 g_action_canceled_statuses[];
extern action_context_e g_action_context;
extern action_current_ability_t g_action_current_ability;
extern s32 g_action_phase;
extern action_command_t g_action_saved_command;
extern action_state_e g_action_state;
extern unit_stats_t* g_action_target;
extern action_data_t* g_action_target_data;
extern s16 g_action_actor_used_ability_id;
extern s32 g_unit_casting_runtime_id;
extern s32 g_reaction_distribute_target_count;
/* Golem barrier HP per team, shared by action resolution and AI snapshots. */
extern u16 g_action_team_golem[UNIT_TEAM_COUNT];
/* Count and coordinates of the tiles already handed out as dismount
 * destinations this action. */
extern s32 g_unit_relocated_count;
extern move_tile_coordinates_t g_unit_relocated_coords[];

/* Preview action calculation entry run on the main stack. */
s32 action_calculate_projected_effect(unit_stats_t* attacker, unit_stats_t* target, action_command_t* command);
void action_clear_current_data(action_data_t* action);
void move_knockback_cancel(void);
void action_execute_ability(void);
void action_finalize_target_result(void);
s32 action_init_current_data(s32 unit_id);
s32 action_apply_target_result(s32 battle_id);
s32 action_resolve_strike(s32 battle_id, action_strike_work_t* work);
s32 action_calculate_target_result(const action_command_t* source, u8 target_id);
void action_apply_strike_results(void);
void action_store_actor_data(unit_stats_t* unit);
void action_store_target_ids(u8* src);
s32 action_apply_attacker_result(s32 unit_id);
void action_store_used_weapon(action_command_t* action);

/* reactions */
/* Reaction behavior flags (g_ability_reaction_behavior_flags). */
typedef enum reaction_behavior_flag {
    REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY = 0x01,
    REACTION_BEHAVIOR_FLAG_ABILITY = 0x02,
    REACTION_BEHAVIOR_FLAG_TARGET_SELF = 0x04,
    REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER = 0x08,
    REACTION_BEHAVIOR_FLAG_PASSIVE = 0x10,
    REACTION_BEHAVIOR_FLAG_ACTIVE = 0x20,
} reaction_behavior_flag_e;

extern s32 g_reaction_hamedo_flag;
extern s16 g_reaction_ability_id;
/* Separate link name for g_action_saved_command.ability_id. */
extern s16 g_action_saved_ability_id;

s32 reaction_fails_brave_roll(const unit_stats_t* unit);
s32 reaction_is_blocked(const unit_stats_t* unit);
s32 reaction_is_prevented(const unit_stats_t* unit);
s32 reaction_prepare_hamedo(s32 id, s16* out_ability);
s32 reaction_prepare_next(s16* out_ability);

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
extern action_mimic_attacker_t g_ai_mimic_attacker_backup;

void action_mimic_record(unit_stats_t* unit);
s32 action_mimic_set_ability(unit_stats_t* unit);

/* result displays */
/* Pending post-action displays are queued as one little-endian 32-bit value in runtime
 * Unit Data 0x1b8..0x1bb. result_build_next_display and
 * result_build_next_special_display consume one bit at a time in ascending
 * order. */
typedef enum action_display_flag {
    ACTION_DISPLAY_FLAG_HP_DAMAGE = 0x00000001,
    ACTION_DISPLAY_FLAG_HP_HEALING = 0x00000002,
    ACTION_DISPLAY_FLAG_MP_DAMAGE = 0x00000004,
    ACTION_DISPLAY_FLAG_MP_HEALING = 0x00000008,
    ACTION_DISPLAY_FLAG_SPEED_DAMAGE = 0x00000010,
    ACTION_DISPLAY_FLAG_SPEED_GAIN = 0x00000020,
    ACTION_DISPLAY_FLAG_CT_DAMAGE = 0x00000040,
    ACTION_DISPLAY_FLAG_CT_GAIN = 0x00000080,
    ACTION_DISPLAY_FLAG_BRAVE_DAMAGE = 0x00000100,
    ACTION_DISPLAY_FLAG_BRAVE_GAIN = 0x00000200,
    ACTION_DISPLAY_FLAG_FAITH_DAMAGE = 0x00000400,
    ACTION_DISPLAY_FLAG_FAITH_GAIN = 0x00000800,
    ACTION_DISPLAY_FLAG_PA_DAMAGE = 0x00001000,
    ACTION_DISPLAY_FLAG_PA_GAIN = 0x00002000,
    ACTION_DISPLAY_FLAG_MA_DAMAGE = 0x00004000,
    ACTION_DISPLAY_FLAG_MA_GAIN = 0x00008000,
    ACTION_DISPLAY_FLAG_LEVEL_DOWN = 0x00010000,
    ACTION_DISPLAY_FLAG_LEVEL_UP = 0x00020000,
    ACTION_DISPLAY_FLAG_STOLEN = 0x00040000,
    ACTION_DISPLAY_FLAG_BROKEN = 0x00080000,
    ACTION_DISPLAY_FLAG_QUICK = 0x00100000,
    ACTION_DISPLAY_FLAG_CT_ZERO = 0x00200000,
    ACTION_DISPLAY_FLAG_MISSED = 0x00400000,
    ACTION_DISPLAY_FLAG_GUARDED = 0x00800000,
    ACTION_DISPLAY_FLAG_CAUGHT = 0x01000000,
    ACTION_DISPLAY_FLAG_GAINED_EXP = 0x02000000,
    ACTION_DISPLAY_FLAG_GAINED_JP = 0x04000000,
    ACTION_DISPLAY_FLAG_NO_TARGET = 0x08000000,
    ACTION_DISPLAY_FLAG_SILENCED = 0x10000000,
    ACTION_DISPLAY_FLAG_NO_MP = 0x20000000,
} action_display_flag_e;

/* Numeric values correspond to the first sixteen action display flags. */
enum {
    ACTION_DISPLAY_VALUE_COUNT = 16,
    ACTION_DISPLAY_NUMERIC_FLAGS_MASK = 0x0000ffff,
};

/* The low 9 bits select a result graphic; bit 9 alters its motion for a
 * critical hit. Values below 0x10 are also indices into
 * g_result_display_values. */
typedef enum result_numeric_display_selector {
    RESULT_NUMERIC_DISPLAY_HP_DAMAGE = 0x000,
    RESULT_NUMERIC_DISPLAY_HP_HEALING = 0x001,
    RESULT_NUMERIC_DISPLAY_MP_DAMAGE = 0x002,
    RESULT_NUMERIC_DISPLAY_MP_HEALING = 0x003,
    RESULT_NUMERIC_DISPLAY_SPEED_DAMAGE = 0x004,
    RESULT_NUMERIC_DISPLAY_SPEED_GAIN = 0x005,
    RESULT_NUMERIC_DISPLAY_CT_DAMAGE = 0x006,
    RESULT_NUMERIC_DISPLAY_CT_GAIN = 0x007,
    RESULT_NUMERIC_DISPLAY_BRAVE_DAMAGE = 0x008,
    RESULT_NUMERIC_DISPLAY_BRAVE_GAIN = 0x009,
    RESULT_NUMERIC_DISPLAY_FAITH_DAMAGE = 0x00a,
    RESULT_NUMERIC_DISPLAY_FAITH_GAIN = 0x00b,
    RESULT_NUMERIC_DISPLAY_PA_DAMAGE = 0x00c,
    RESULT_NUMERIC_DISPLAY_PA_GAIN = 0x00d,
    RESULT_NUMERIC_DISPLAY_MA_DAMAGE = 0x00e,
    RESULT_NUMERIC_DISPLAY_MA_GAIN = 0x00f,
    RESULT_NUMERIC_DISPLAY_MISSED = 0x010,
    RESULT_NUMERIC_DISPLAY_GUARDED = 0x020,
    RESULT_NUMERIC_DISPLAY_CAUGHT = 0x030,
    RESULT_NUMERIC_DISPLAY_GAINED_EXP = 0x040,
    RESULT_NUMERIC_DISPLAY_GAINED_JP = 0x050,
    RESULT_NUMERIC_DISPLAY_STATUS_INFLICTED = 0x060,
    RESULT_NUMERIC_DISPLAY_STATUS_REMOVED = 0x070,
    RESULT_NUMERIC_DISPLAY_LEVEL_DOWN = 0x080,
    RESULT_NUMERIC_DISPLAY_LEVEL_UP = 0x090,
    RESULT_NUMERIC_DISPLAY_STOLEN = 0x0a0,
    RESULT_NUMERIC_DISPLAY_BROKEN = 0x0b0,
    RESULT_NUMERIC_DISPLAY_QUICK = 0x0c0,
    RESULT_NUMERIC_DISPLAY_CT_ZERO = 0x0d0,
    RESULT_NUMERIC_DISPLAY_NO_TARGET = 0x0e0,
    RESULT_NUMERIC_DISPLAY_SILENCED = 0x0f0,
    RESULT_NUMERIC_DISPLAY_NO_MP = 0x100,
} result_numeric_display_selector_e;

/* Encoding in action_result_display_t.selector. */
enum {
    RESULT_NUMERIC_DISPLAY_SELECTOR_MASK = 0x1ff,
    RESULT_NUMERIC_DISPLAY_MODIFIER_CRITICAL = 0x200,
};

/* Image IDs used by the projected-action display. These index the 49-entry
 * g_result_image_rects; they are not battle status IDs. */
typedef enum action_display_image_id {
    ACTION_DISPLAY_IMAGE_DEAD = 0,
    ACTION_DISPLAY_IMAGE_UNDEAD = 1,
    ACTION_DISPLAY_IMAGE_PETRIFY = 2,
    ACTION_DISPLAY_IMAGE_INVITE = 3,
    ACTION_DISPLAY_IMAGE_DARKNESS = 4,
    ACTION_DISPLAY_IMAGE_CONFUSION = 5,
    ACTION_DISPLAY_IMAGE_SILENCE = 6,
    ACTION_DISPLAY_IMAGE_BLOOD_SUCK = 7,
    ACTION_DISPLAY_IMAGE_OIL = 8,
    ACTION_DISPLAY_IMAGE_FLOAT = 9,
    ACTION_DISPLAY_IMAGE_RERAISE = 10,
    ACTION_DISPLAY_IMAGE_TRANSPARENT = 11,
    ACTION_DISPLAY_IMAGE_BERSERK = 12,
    ACTION_DISPLAY_IMAGE_POISON = 13,
    ACTION_DISPLAY_IMAGE_REGEN = 14,
    ACTION_DISPLAY_IMAGE_PROTECT = 15,
    ACTION_DISPLAY_IMAGE_SHELL = 16,
    ACTION_DISPLAY_IMAGE_HASTE = 17,
    ACTION_DISPLAY_IMAGE_SLOW = 18,
    ACTION_DISPLAY_IMAGE_STOP = 19,
    ACTION_DISPLAY_IMAGE_FAITH = 20,
    ACTION_DISPLAY_IMAGE_INNOCENT = 21,
    ACTION_DISPLAY_IMAGE_CHARM = 22,
    ACTION_DISPLAY_IMAGE_SLEEP = 23,
    ACTION_DISPLAY_IMAGE_DONT_MOVE = 24,
    ACTION_DISPLAY_IMAGE_DONT_ACT = 25,
    ACTION_DISPLAY_IMAGE_REFLECT = 26,
    ACTION_DISPLAY_IMAGE_DEATH_SENTENCE = 27,
    ACTION_DISPLAY_IMAGE_STOLEN = 28,
    ACTION_DISPLAY_IMAGE_BROKEN = 29,
    ACTION_DISPLAY_IMAGE_QUICK = 36,
    ACTION_DISPLAY_IMAGE_HP = 37,
    ACTION_DISPLAY_IMAGE_MP = 38,
    ACTION_DISPLAY_IMAGE_CT = 39,
    ACTION_DISPLAY_IMAGE_SPEED = 40,
    ACTION_DISPLAY_IMAGE_BRAVE = 41,
    ACTION_DISPLAY_IMAGE_FAITH_STAT = 42,
    ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK = 43,
    ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK = 44,
    ACTION_DISPLAY_IMAGE_LEVEL = 45,
    ACTION_DISPLAY_IMAGE_GIL = 46,
    ACTION_DISPLAY_IMAGE_EXP = 47,
    ACTION_DISPLAY_IMAGE_FROG = 48,
    ACTION_DISPLAY_IMAGE_NONE = 255,
} action_display_image_id_e;

enum { ACTION_DISPLAY_IMAGE_COUNT = ACTION_DISPLAY_IMAGE_FROG + 1 };

typedef struct action_display_image_rect {
    u8 u;
    u8 v;
    u8 width;
    u8 height;
} action_display_image_rect_t;
typedef char action_display_image_rect_size_must_be_4[(sizeof(action_display_image_rect_t) == 4) ? 1 : -1];

/* Ordinary SP/PA/MA/Brave/Faith action-result encoding. CT and EXP reuse
 * these bits with distinct sentinel or sign meanings and are excluded. */
enum {
    ACTION_STAT_CHANGE_VALUE_MASK = 0x7f,
    ACTION_STAT_CHANGE_INCREASE = 0x80,
};

/* action_data_t.exp_change: the low seven bits are the amount, and the top bit
 * marks a loss, the opposite sense of ACTION_STAT_CHANGE_INCREASE. */
enum {
    ACTION_EXP_CHANGE_VALUE_MASK = 0x7f,
    ACTION_EXP_CHANGE_LOSS = 0x80,
};

/* g_reward_report_phase: the reports shown one after another once an action
 * has resolved. The post-movement reports use the first two. */
typedef enum reward_report_phase {
    REWARD_REPORT_LEVEL_UP = 0,
    REWARD_REPORT_JOB_LEVEL_UP = 1,
    REWARD_REPORT_LEARN_ON_HIT = 2,
} reward_report_phase_e;

/* Queue of post-action "effect message" popups (g_action_messages), filled by
 * action_message_queue_for_unit and drained newest first by
 * action_message_show_next. */
typedef struct action_post_effect_msg {
    u8 code;           /* 0x00 action_display_image_id_e of the popup */
    u8 unit;           /* 0x01 battle id of the reported unit */
    u8 _padding_02[2]; /* 0x02; aligns value */
    s32 value;         /* 0x04 reported amount */
} action_post_effect_msg_t;
typedef char battle_post_effect_msg_size_must_be_8[(sizeof(action_post_effect_msg_t) == 8) ? 1 : -1];

enum { BATTLE_POST_EFFECT_MSG_CAPACITY = 16 };

extern action_display_image_rect_t g_result_image_rects[ACTION_DISPLAY_IMAGE_COUNT];
extern const u8 g_result_numeric_selectors[ACTION_DISPLAY_VALUE_COUNT];
extern u16 g_result_display_values[ACTION_DISPLAY_VALUE_COUNT];
extern s32 g_ability_learn_on_hit_unit_id;
extern s32 g_state_window_closed;
extern s32 g_reward_report_phase; /* reward_report_phase_e */
extern action_post_effect_msg_t g_action_messages[BATTLE_POST_EFFECT_MSG_CAPACITY];
extern const palette_t g_result_palette;
/* Eight per status_changes mask byte, indexed from the byte's high bit. */
extern u8 g_result_status_image_ids[STATUS_BYTE_COUNT][8];

void state_enter_learn_ability_on_hit(void);
void action_post_action_facing_stub(unit_stats_t* unused_unit, s32 unused_facing);
void action_message_show_for_caster(void);
s32 action_message_show_next(void);
void action_start_next_strike(void);

/* rewards and items */
/* Action result limits: HP and MP damage and healing stop at 999. The EXP an
 * action earns starts at 10 for a success or 20 divided by the KO count for a
 * kill, and ends within 1..99. Poach yields the rare item 31 times in 256. */
enum {
    ACTION_VALUE_MAX = 999,
    REWARD_EXP_SUCCESS_BASE = 10,
    REWARD_EXP_KILL_BASE = 20,
    REWARD_EXP_MAX = 99,
    ITEM_POACH_RARE_ITEM_CHANCE = 31,
};

/* Sum and highest level of the units of any team but blue, counted as the ENTD units load and as
 * an off-field unit is added; later level changes are not counted. They set the level-sum and
 * level-bonus money. */
extern u16 g_reward_enemy_level_sum;
extern u8 g_reward_highest_enemy_level;

s32 reward_add_war_funds(unit_stats_t* unit, s32 total, u8 item_id);
void reward_apply_exp_change(unit_stats_t* unit, u8 amount);
s32 move_find_apply_trap(unit_stats_t* unit);
s32 item_decrement_party_quantity(unit_stats_t* unit, s32 item_id, s32 spend);
s32 item_give(unit_stats_t* unit, u8 item);
void reward_grant(void);
s32 move_init_ability_benefit(unit_stats_t* unit);

#endif
