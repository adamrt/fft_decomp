#ifndef BATTLE_ACTION_H
#define BATTLE_ACTION_H

/* Battle action preparation, execution and result processing. */

#include "fft/battle_effect.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

/* turn types */
/* action_run_between_turn_control modes. */
typedef enum battle_between_turn_mode {
    BATTLE_BETWEEN_TURN_MODE_RUN = 0,
    BATTLE_BETWEEN_TURN_MODE_RESET = 1,
    BATTLE_BETWEEN_TURN_MODE_FIND_ACTIVE_UNIT = 2,
} battle_between_turn_mode_e;

typedef enum battle_turn_status_flags {
    BATTLE_TURN_STATUS_CT_FROZEN = 0x01,
    BATTLE_TURN_STATUS_INCAPACITATED = 0x02,
    BATTLE_TURN_STATUS_DEAD = 0x04,
    BATTLE_TURN_STATUS_SLEEP = 0x08,
    BATTLE_TURN_STATUS_HASTE = 0x10,
    BATTLE_TURN_STATUS_SLOW = 0x20,
    BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK = 0x0f,
    BATTLE_TURN_STATUS_BLOCKS_TURN_MASK = 0x0d,
} battle_turn_status_flags_e;

typedef enum battle_turn_event {
    BATTLE_TURN_EVENT_UNIT_READY = 0x0100,
    BATTLE_TURN_EVENT_ABILITY_READY = 0x0200,
    BATTLE_TURN_EVENT_ACTION_RESULT = 0x0300,
    BATTLE_TURN_EVENT_UNKNOWN_0400 = 0x0400,
    BATTLE_TURN_EVENT_MIME = 0x0500,
    BATTLE_TURN_EVENT_NONE = 0xff00,
} battle_turn_event_e;

/* g_battle_between_turn_state and g_battle_between_turn_resume_state: the phases of
 * action_run_between_turn_control. */
typedef enum battle_between_turn_state {
    BATTLE_BETWEEN_TURN_STATE_ADD_CT = 0,         /* every unit gains CT by its speed */
    BATTLE_BETWEEN_TURN_STATE_PICK_UNIT = 1,      /* the highest CT of 100 or more takes a turn */
    BATTLE_BETWEEN_TURN_STATE_TICK_CHARGES = 2,   /* count down every charged_ability_ct */
    BATTLE_BETWEEN_TURN_STATE_RESOLVE_CHARGE = 3, /* a charge that reached 0 resolves */
    BATTLE_BETWEEN_TURN_STATE_EXPIRE_STATUSES = 4,
    BATTLE_BETWEEN_TURN_STATE_COLLECT_PENDING_ACTIONS = 5, /* note the units whose action has an attack type */
    BATTLE_BETWEEN_TURN_STATE_RUN_PENDING_ACTIONS = 6,
    BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE = 9, /* a unit whose CT is 0xff takes a turn at once */
    BATTLE_BETWEEN_TURN_STATE_UNIT_TURN = 10,             /* end the turn of each unit that has one */
    BATTLE_BETWEEN_TURN_STATE_MARK_MIMES = 13,
    BATTLE_BETWEEN_TURN_STATE_RUN_MIMES = 14,
    BATTLE_BETWEEN_TURN_STATE_TERRAIN_POISON = 15,
    BATTLE_BETWEEN_TURN_STATE_REMOVE_TRANSPARENT = 17,
    BATTLE_BETWEEN_TURN_STATE_POISON_AND_REGEN = 19,
} battle_between_turn_state_e;

/* action results */
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
    ACTION_SPECIAL_EFFECT_NULLIFIED = 0x0200,
    ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION = 0x0400,
    ACTION_SPECIAL_EFFECT_ELEMENTAL_WEAKNESS = 0x0800,
    ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN = 0x1000,
    ACTION_SPECIAL_EFFECT_KNOCKBACK = 0x4000,
    ACTION_SPECIAL_EFFECT_GOLEM_GUARD = 0x8000,
} action_special_effect_e;

/* strike state */
typedef enum action_context {
    ACTION_CONTEXT_PRIMARY = 0,
    ACTION_CONTEXT_REACTION_OR_SIMULATION = 1,
} action_context_e;

typedef enum action_state {
    ACTION_STATE_EXECUTE = 0,
    ACTION_STATE_AI_SIMULATION = 1,
    ACTION_STATE_PREVIEW = 2,
} action_state_e;

/* g_battle_action_phase: which unit's action the attack phase is announcing. */
typedef enum action_phase {
    ACTION_PHASE_PRE_ACTION = 0,    /* a reaction that pre-empts the pending action (Hamedo) */
    ACTION_PHASE_ACTING_UNIT = 1,   /* the acting unit's own action */
    ACTION_PHASE_REACTING_UNIT = 2, /* the reactions it provoked */
} action_phase_e;

enum { ACTION_PHASE_COUNT = 3 };

/* result display types */
/* Pending post-action displays are queued as one little-endian 32-bit value in runtime
 * Unit Data 0x1b8..0x1bb.  The two dispatchers at 0x8007f5f8 and 0x800808b8
 * consume one bit at a time in ascending order. */
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

/* The low 9 bits select a result graphic; bit 9 alters its motion for a
 * critical hit.  Values below 0x10 are also indices into the numeric-value
 * table at 0x800962d4. */
typedef enum battle_numeric_display_selector {
    BATTLE_NUMERIC_DISPLAY_HP_DAMAGE = 0x000,
    BATTLE_NUMERIC_DISPLAY_HP_HEALING = 0x001,
    BATTLE_NUMERIC_DISPLAY_MP_DAMAGE = 0x002,
    BATTLE_NUMERIC_DISPLAY_MP_HEALING = 0x003,
    BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE = 0x004,
    BATTLE_NUMERIC_DISPLAY_SPEED_GAIN = 0x005,
    BATTLE_NUMERIC_DISPLAY_CT_DAMAGE = 0x006,
    BATTLE_NUMERIC_DISPLAY_CT_GAIN = 0x007,
    BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE = 0x008,
    BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN = 0x009,
    BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE = 0x00a,
    BATTLE_NUMERIC_DISPLAY_FAITH_GAIN = 0x00b,
    BATTLE_NUMERIC_DISPLAY_PA_DAMAGE = 0x00c,
    BATTLE_NUMERIC_DISPLAY_PA_GAIN = 0x00d,
    BATTLE_NUMERIC_DISPLAY_MA_DAMAGE = 0x00e,
    BATTLE_NUMERIC_DISPLAY_MA_GAIN = 0x00f,
    BATTLE_NUMERIC_DISPLAY_MISSED = 0x010,
    BATTLE_NUMERIC_DISPLAY_GUARDED = 0x020,
    BATTLE_NUMERIC_DISPLAY_CAUGHT = 0x030,
    BATTLE_NUMERIC_DISPLAY_GAINED_EXP = 0x040,
    BATTLE_NUMERIC_DISPLAY_GAINED_JP = 0x050,
    BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED = 0x060,
    BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED = 0x070,
    BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN = 0x080,
    BATTLE_NUMERIC_DISPLAY_LEVEL_UP = 0x090,
    BATTLE_NUMERIC_DISPLAY_STOLEN = 0x0a0,
    BATTLE_NUMERIC_DISPLAY_BROKEN = 0x0b0,
    BATTLE_NUMERIC_DISPLAY_QUICK = 0x0c0,
    BATTLE_NUMERIC_DISPLAY_CT_ZERO = 0x0d0,
    BATTLE_NUMERIC_DISPLAY_NO_TARGET = 0x0e0,
    BATTLE_NUMERIC_DISPLAY_SILENCED = 0x0f0,
    BATTLE_NUMERIC_DISPLAY_NO_MP = 0x100,
} battle_numeric_display_selector_e;

/* Encoding in action_result_display_t.selector. */
enum {
    BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK = 0x1ff,
    BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL = 0x200,
};

/* Numeric values correspond to the first sixteen action display flags. */
enum {
    ACTION_DISPLAY_VALUE_COUNT = 16,
    ACTION_DISPLAY_NUMERIC_FLAGS_MASK = 0x0000ffff,
};

/* Image IDs used by the projected-action display. These index the 49-entry
 * texture-source table at 0x8014cf68; they are not battle status IDs. */
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

/* strike data */
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

/* Current ability data at 0x801938c0: the attacker/target pair, weapons, formula
 * inputs and results of the strike being resolved, with copies of the ability's
 * secondary data, the weapon's data and the status-infliction record that
 * action_run_pre_formula_setup loads. */
typedef struct battle_current_ability {
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
    u8 mp_cost;                                 /* 0x2b: effective cost recorded before the MP checks at
                                                 * 0x8017db74; reaction eligibility reads it even when no
                                                 * MP was subtracted */
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
} battle_current_ability_t;
typedef char battle_current_ability_size_must_be_80[(sizeof(battle_current_ability_t) == 80) ? 1 : -1];

/* battle_current_ability_t.knockback_flags: the kind of knockback found by
 * formula_calculate_knockback, with PENDING set until it is applied. */
typedef enum battle_knockback_kind {
    BATTLE_KNOCKBACK_FALL_DAMAGE = 1,    /* a grounded target: the fall height can hurt */
    BATTLE_KNOCKBACK_NO_FALL_DAMAGE = 2, /* a flying target */
} battle_knockback_kind_e;

/* Encoding of the kind and pending marker in the same byte. */
enum {
    BATTLE_KNOCKBACK_KIND_MASK = 0x7f,
    BATTLE_KNOCKBACK_PENDING = 0x80,
};

/* battle_current_ability_t.post_action_message, copied to the strike work and
 * the runtime unit record: the message the post-action state opens. */
typedef enum battle_post_action_message {
    BATTLE_POST_ACTION_MESSAGE_NONE = 0,
    BATTLE_POST_ACTION_MESSAGE_POACHED = 6,
    BATTLE_POST_ACTION_MESSAGE_TRAINED = 9,
} battle_post_action_message_e;

/* mimic and reaction types */
/* Current attacker record at 0x8019389c, filled by
 * action_set_current_attacker_data and read by
 * action_set_mimic_ability. The AI saves and restores it whole as 30
 * bytes (ai_begin_action_simulation / ai_end_action_simulation). */
typedef struct battle_current_attacker_data {
    u8 facing;               /* 0x00 */
    u8 mimic_pending;        /* 0x01: a non-Mime action was recorded; state 13 queues the Mimes */
    u8 right_hand_weapon_id; /* 0x02 */
    u8 left_hand_weapon_id;  /* 0x03 */
    u8 team;                 /* 0x04 */
    u8 _padding_05;          /* 0x05; aligns target_delta_x */
    s16 target_delta_x;      /* 0x06 */
    s16 target_delta_y;      /* 0x08 */
    action_command_t action; /* 0x0a: copy of unit_stats_t +0x16e */
} battle_current_attacker_data_t;
typedef char battle_current_attacker_data_size_must_be_30[(sizeof(battle_current_attacker_data_t) == 30) ? 1 : -1];

/* Reaction behavior flags (g_main_reaction_behavior_flags). */
typedef enum battle_reaction_behavior_flag {
    BATTLE_REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY = 0x01,
    BATTLE_REACTION_BEHAVIOR_FLAG_ABILITY = 0x02,
    BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_SELF = 0x04,
    BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER = 0x08,
    BATTLE_REACTION_BEHAVIOR_FLAG_PASSIVE = 0x10,
    BATTLE_REACTION_BEHAVIOR_FLAG_ACTIVE = 0x20,
} battle_reaction_behavior_flag_e;

/* action processing modes */
/* action_calculate_at_list modes. */
typedef enum battle_at_list_mode {
    BATTLE_AT_LIST_MODE_ALL = 0,
    BATTLE_AT_LIST_MODE_TURNS_ONLY = 1,   /* leave out the charged and performing actions */
    BATTLE_AT_LIST_MODE_ACTIONS_ONLY = 2, /* leave out the active unit and the simulated turns */
    BATTLE_AT_LIST_MODE_PREVIEW = 3,      /* as ALL, with the acting unit's new charge counted from this tick */
} battle_at_list_mode_e;

/* Result of action_prepare_attack, kept in battle_unit_t.attack_phase_state. */
typedef enum battle_attack_preparation {
    BATTLE_ATTACK_PREPARATION_INVALID = -1,
    BATTLE_ATTACK_PREPARATION_CHARGE = 0, /* the action has a charge time; also Defend */
    BATTLE_ATTACK_PREPARATION_IMMEDIATE = 1,
    BATTLE_ATTACK_PREPARATION_CHANGE_EQUIPMENT = 2,
    BATTLE_ATTACK_PREPARATION_JUMP = 3,
} battle_attack_preparation_e;

/* g_battle_action_post_action_display_phase: the reports shown one after another once an action
 * has resolved. The post-movement reports use the first two. */
typedef enum battle_post_action_display_phase {
    BATTLE_POST_ACTION_DISPLAY_LEVEL_UP = 0,
    BATTLE_POST_ACTION_DISPLAY_JOB_LEVEL_UP = 1,
    BATTLE_POST_ACTION_DISPLAY_LEARN_ON_HIT = 2,
} battle_post_action_display_phase_e;

/* turns */
extern s32 g_battle_action_turn_event_type;
extern u8 g_battle_acting_unit_elevation;
extern s32 g_battle_acting_unit_id;
extern u8 g_battle_acting_unit_x;
extern u8 g_battle_acting_unit_y;
extern s32 g_battle_between_turn_resume_state;
extern s32 g_battle_between_turn_state;
extern s32 g_battle_state_wait_direction_facing;
extern s32 g_battle_turn_unit_id;

s32 battle_action_check_battle_outcome(void);
void battle_action_check_between_turn_events(void);
s32 battle_action_check_change_of_turn(s32 unit_id);
void battle_action_choose_facing_for_wait(void);
void battle_action_choose_wait(void);
s32 battle_action_end_turn(s32 unit_id);
s32 battle_action_run_between_turn_control(battle_between_turn_mode_e mode);

/* active time list */
extern s32 g_battle_action_at_list_active;
extern s32 g_battle_action_at_list_id;

s32 battle_action_calculate_at_list(battle_at_entry_t* list, battle_at_list_mode_e mode);
s32 battle_action_get_at_list_entry_unit_id(s32 at_index);
s32 battle_action_find_unit_turn_index(const unit_stats_t* unit);
void battle_action_clear_at_list_active(void);
void battle_action_clear_at_list_id(void);
void battle_action_get_next_acting_unit(s32 arg);
u32 battle_action_get_number_of_turns_to_resolve(s32 battle_id, s32 ct, const battle_at_entry_t* entries);
void battle_action_open_status_menu_at_list_start(void);
s32 battle_action_preview_at_list(unit_stats_t* unit, action_command_t* action, battle_at_entry_t* at_list);
void battle_action_set_at_list_active(void);
void battle_action_show_next_at_list_entry(void);

/* preparation */
extern u8 g_action_menu_row_types[]; /* per-row skillset kind; 13 keeps the row active */

void battle_action_call_attack_preparation(action_command_t* action);
s32 battle_action_call_attack_preparation_at_preview(action_command_t* command);
void battle_action_confirm(void);
s32 battle_action_get_elemental_ability_id(const unit_stats_t* unit);
s32 battle_action_prepare_attack(action_command_t* action, action_command_t* target, s32 flag);
void battle_action_set_casting_unit_id_ff(void);
void battle_action_set_casting_unit_id_ff_and_init(void);
void battle_action_switch_ability_to_default_attack(void);

/* strikes */
extern unit_stats_t* g_battle_action_attacker;
extern action_data_t g_battle_action_attacker_action_data;
extern action_data_t* g_battle_action_attacker_data;
extern u8 g_battle_action_canceled_statuses[];
extern action_context_e g_battle_action_context;
extern battle_current_ability_t g_battle_action_current_ability;
extern s32 g_battle_action_phase;
extern action_command_t g_battle_action_saved_command;
extern action_state_e g_battle_action_state;
extern unit_stats_t* g_battle_action_target;
extern action_data_t* g_battle_action_target_data;
extern s16 g_battle_acting_unit_used_ability_id;
extern s32 g_battle_casting_runtime_id;
/* Golem barrier HP per team, shared by action resolution and AI snapshots. */
extern u16 g_battle_team_golem[BATTLE_TEAM_COUNT];
extern s32 g_battle_unit_source_runtime_id;

/* Preview action calculation entry run on the main stack. */
s32 battle_action_calculate_projected_effect(unit_stats_t* attacker, unit_stats_t* target, action_command_t* command);
void battle_action_clear_current_data(action_data_t* action);
void battle_action_clear_knockback_flag(void);
void battle_action_execute_ability(void);
void battle_action_finalize_target_current_action(void);
s32 battle_action_init_current_data(s32 unit_id);
s32 battle_action_resolve_ability_strike(s32 battle_id, battle_strike_work_t* work);
s32 battle_action_resume_attack_phase_control(void);
s32 battle_action_run_pre_formula_setup(const action_command_t* source, u8 target_id);
void battle_action_set_target_coords_and_attacker_anim(void);
void battle_action_store_acting_unit_data(unit_stats_t* unit);
void battle_action_store_target_ids(u8* src);
s32 battle_action_store_target_stats_pointer_data(s32 unit_id);
void battle_action_store_used_weapon(action_command_t* action);

/* reactions */
extern s32 g_battle_action_hamedo_flag;
extern s32 g_battle_action_post_effect_msg_counter;
extern s16 g_battle_current_reaction_ability_id;
extern s32 g_battle_distribute_target_count;
/* Separate link name for g_battle_action_saved_command.ability_id. */
extern s16 g_battle_reaction_unit_last_ability_id;

s32 battle_action_calculate_chance_to_react(const unit_stats_t* unit);
s32 battle_action_is_reaction_prevented(const unit_stats_t* unit);
s32 battle_action_finalize_attack_and_flag_reactions(s32 runtime_id);
s32 battle_action_is_reaction_blocked(const unit_stats_t* unit);
s32 battle_reaction_prepare_hamedo_for_pending_action(s32 id, s16* out_ability);
s32 battle_reaction_prepare_next(s16* reaction_id);

/* mimic */
extern battle_current_attacker_data_t g_battle_action_current_attacker;
extern battle_current_attacker_data_t g_battle_ai_current_attacker_backup;

void battle_action_set_current_attacker_data(unit_stats_t* unit);
s32 battle_action_set_mimic_ability(unit_stats_t* unit);

/* results and rewards */
extern action_display_image_rect_t g_battle_action_display_image_rects[ACTION_DISPLAY_IMAGE_COUNT];
extern const u8 g_battle_action_display_numeric_selectors[ACTION_DISPLAY_VALUE_COUNT];
extern u16 g_battle_action_display_values[ACTION_DISPLAY_VALUE_COUNT];
extern s32 g_battle_action_learn_on_hit_unit_id;
extern s32 g_battle_action_post_action;
extern s32 g_battle_action_post_action_display_phase; /* battle_post_action_display_phase_e */
extern battle_post_effect_msg_t g_battle_action_post_effect_msgs[];
extern const palette_t g_battle_action_result_palette;
/* Eight per status_changes mask byte, indexed from the byte's high bit. */
extern u8 g_battle_action_status_display_image_ids[STATUS_BYTE_COUNT][8];
extern u8 g_battle_mimic_pending_flags[21];

s32 battle_action_add_war_funds(unit_stats_t* unit, s32 total, u8 item_id);
s32 battle_action_apply_tile_trap(unit_stats_t* unit);
s32 battle_action_decrement_player_item_quantity(unit_stats_t* unit, s32 item_id, s32 always_one);
void battle_action_handle_move_command(void);
void battle_action_handle_post_action_xp_jp_ability(void);
void battle_action_handle_steal_exp(unit_stats_t* unit, u8 amount);
s32 battle_action_increment_item_quantity_for_steal_break(unit_stats_t* unit, u8 item);
void battle_action_init_learn_ability_on_hit(void);
s32 battle_action_init_movement_ability_benefit(unit_stats_t* unit);
void battle_action_post_action_facing_stub(unit_stats_t* unused_unit, s32 unused_facing);
void battle_action_start_next_strike_display(void);
void battle_action_set_item_throw_stone_ability_display(void);
void battle_action_set_only_action_taken(s32 unit_id);
void battle_action_set_only_movement_taken(s32 unit_id);
void battle_action_show_caster_post_effect_messages(void);

#endif
