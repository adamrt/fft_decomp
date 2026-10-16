#ifndef BATTLE_AI_H
#define BATTLE_AI_H

/* Battle AI workspace, ability evaluation and action selection. */

#include "fft/data.h"
#include "fft/data_ability.h"
#include "fft/data_item.h"
#include "fft/map.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "psx/types.h"

/* workspace limits */
/* Fixed workspace capacities and tile-grid storage, independent of map bounds. */
enum {
    AI_COMPACT_UNIT_COUNT = 16,
    AI_ABILITY_LIST_COUNT = 34,
    AI_RANKED_ACTION_COUNT = 8,
    AI_MOVEMENT_SCENARIO_COUNT = 3,
    AI_KNOWN_TRAP_COUNT = 4,
    AI_GRID_ROW_COUNT = 18,
    AI_GRID_COLUMN_COUNT = 16,
};

enum {
    AI_MATH_SOURCE_SKILLSET_COUNT = 4,
    AI_MATH_EXTRA_KNOWN_ABILITY_COUNT = 10,
};

/* Scanlines since vertical blank at which each search yields. */
enum {
    AI_TURN_SIMULATION_SCANLINE_LIMIT = 313,
    AI_PATHFIND_SCANLINE_LIMIT = 325,
    AI_DECISION_SCANLINE_LIMIT = 441,
};

/* Turn-event iteration limit for one simulated action. */
enum {
    AI_SIMULATED_TURN_LIMIT = 256,
};

/* Shares of usable abilities are measured in 128ths. */
enum {
    AI_ABILITY_SHARE_SCALE = 128,
    AI_SUPPORT_SHARE_THRESHOLD = 77,
};

/* ability flags */
/* The four AI flag bytes of an ability, one enum per byte. */
enum {
    AI_ABILITY_FLAG_1_TARGET_ALLIES = 0x01,
    AI_ABILITY_FLAG_1_TARGET_ENEMIES = 0x02,
    AI_ABILITY_FLAG_1_AFFECTS_HP = 0x80,
};

enum {
    AI_ABILITY_FLAG_2_EVADEABLE = 0x02,
    AI_ABILITY_FLAG_2_UNDEAD_REVERSE = 0x20,
    AI_ABILITY_FLAG_2_TARGET_MAP_TILES = 0x80,
};

enum {
    AI_ABILITY_FLAG_3_PHYSICAL_ATTACK = 0x01,
    AI_ABILITY_FLAG_3_MAGICAL_ATTACK = 0x02,
    /* Four area-shape bits, tested only together: 0x20 a line from the user
     * (AI_ABILITY_LINEAR_TRAJECTORY in the 32-bit view), 0x10 the eight
     * breath and bracelet cones, 0x08 Triple Attack alone, 0x04 Triple Bracelet
     * alone. With any of them set the AI tries every origin tile. */
    AI_ABILITY_FLAG_3_EXHAUSTIVE_ORIGIN_MASK = 0x3c,
    AI_ABILITY_FLAG_3_DIRECT_TRAJECTORY = 0x40,
    AI_ABILITY_FLAG_3_ARC_TRAJECTORY = 0x80,
};

enum {
    AI_ABILITY_FLAG_4_EVADE_WITH_MOTION = 0x01, /* the static table's bit; the AI working copy reuses byte 4 */
    AI_ABILITY_FLAG_4_USABLE_BY_AI = 0x80,
};

/* 32-bit flags of the considered ability. Bytes 1..3 retain the static table
 * meanings, but byte 4 is replaced by ai_ability_entry_flag_e: its
 * low bit means CONSUME_EVALUATED_ORIGINS, not the static EVADE_WITH_MOTION.
 * Team preferences remain distinct from the absolute ONLY restrictions. */
enum {
    AI_ABILITY_TARGET_ALLIES = 0x00000001,
    AI_ABILITY_TARGET_ENEMIES = 0x00000002,
    AI_ABILITY_UNEQUIP = 0x00000004,
    AI_ABILITY_AFFECTS_STATS = 0x00000008,
    AI_ABILITY_ADD_STATUS = 0x00000010,
    AI_ABILITY_CANCEL_STATUS = 0x00000020,
    AI_ABILITY_AFFECTS_MP = 0x00000040,
    AI_ABILITY_AFFECTS_HP = 0x00000080,
    AI_ABILITY_AFFECTED_BY_SILENCE = 0x00000100,
    AI_ABILITY_EVADEABLE = 0x00000200,
    AI_ABILITY_AFFECTED_BY_FAITH = 0x00000400,
    AI_ABILITY_RANDOM_USE = 0x00000800,
    AI_ABILITY_FOLLOW_TARGET = 0x00001000,
    AI_ABILITY_UNDEAD_REVERSE = 0x00002000,
    AI_ABILITY_REFLECTABLE = 0x00004000,
    AI_ABILITY_TARGET_MAP_TILES = 0x00008000,
    AI_ABILITY_PHYSICAL_ATTACK = 0x00010000,
    AI_ABILITY_MAGICAL_ATTACK = 0x00020000,
    AI_ABILITY_LINEAR_TRAJECTORY = 0x00200000,
    AI_ABILITY_DIRECT_TRAJECTORY = 0x00400000,
    AI_ABILITY_ARC_TRAJECTORY = 0x00800000,
    AI_ABILITY_CONSUME_EVALUATED_ORIGINS = 0x01000000,
    AI_ABILITY_EXHAUSTIVE_ORIGIN_SCAN = 0x04000000,
    AI_ABILITY_REQUIRES_MONSTER_SKILL = 0x08000000,
    AI_ABILITY_ENEMIES_ONLY = 0x20000000,
    AI_ABILITY_ALLIES_ONLY = 0x40000000,
    AI_ABILITY_USABLE_BY_AI = 0x80000000,
};

/* coordinate and ability records */
/* X / map elevation / Y / zero, written in that order by ai_copy_unit_coordinates
 * and read as one 32-bit value for equality tests. */
typedef union ai_coords {
    u32 word;
    struct {
        u8 x;
        u8 elevation;
        u8 y;
        u8 zero;
    } fields;
} ai_coords_t;

/* Ability AI behavior flags 1..4. Consumers read the 32-bit value, two 16-bit values,
 * or individual bytes; use the corresponding masks above for each view. */
typedef union ai_ability_flags {
    u32 word;
    ability_ai_flags_t fields;
    struct {
        u16 target_effect_flags;    /* bytes 1..2 */
        u16 trajectory_usage_flags; /* bytes 3..4 */
    } groups;
} ai_ability_flags_t;

/* Shared prefix of the baseline and suspended AI unit snapshots.
 * Byte 2 holds Death Sentence CT only in the compact suspended record;
 * the baseline uses status_ct[15] and leaves this byte untouched. */
typedef struct ai_status_snapshot {
    u8 entd_slot;
    u8 death_counter;
    u8 death_sentence_ct; /* see 0x8019e214 */
    u8 current_status[STATUS_BYTE_COUNT];
} ai_status_snapshot_t;
typedef char ai_status_snapshot_must_be_8[sizeof(ai_status_snapshot_t) == 8 ? 1 : -1];

/* Selected unit state saved at 0x8019e160 and restored at 0x8019e378.
 * HP occupies unaligned bytes 0x1b..0x1c; max HP/MP are not saved. */
typedef struct ai_extended_snapshot {
    ai_status_snapshot_t status;              /* 0x00 */
    u8 status_ct[BATTLE_TIMED_STATUS_COUNT];  /* 0x08 */
    u8 team_flags;                            /* 0x18 */
    u8 faith;                                 /* 0x19 */
    u8 transparent_removal_flag;              /* 0x1a */
    u8 hp_bytes[2];                           /* 0x1b */
    u8 _padding_1d;                           /* aligns mp */
    u16 mp;                                   /* 0x1e */
    u8 charged_ability_ct;                    /* 0x20 */
    u8 base_attributes[UNIT_ATTRIBUTE_COUNT]; /* 0x21; unit_attribute_index_e */
    u8 attributes[UNIT_ATTRIBUTE_COUNT];      /* 0x24; unit_attribute_index_e */
    u8 ct;                                    /* 0x27 */
    u8 x;                                     /* 0x28 */
    u8 position_bytes[2];                     /* 0x29; packed unit position */
    u8 has_turn;                              /* 0x2b */
    u8 movement_taken;
    u8 action_taken;
    u8 auto_battle_setting; /* 0x2e */
    u8 auto_battle_target;
    u8 initial_team_flags;
    u8 inflicted_status[STATUS_BYTE_COUNT];  /* 0x31 */
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT]; /* 0x36 */
    u8 brave;                                /* 0x3d */
    u8 mount_info;
    u8 _padding_3f; /* tail padding to 2-byte alignment */
} ai_extended_snapshot_t;
typedef char ai_extended_snapshot_must_be_64[sizeof(ai_extended_snapshot_t) == 64 ? 1 : -1];

/* AI ability +0x04..0x0b: byte parameters and an aligned 32-bit status tail.
 * 0x8019e9f4 loads status bytes 1..4 together; reading only one byte would
 * silently lose the high status bits tested by the decision evaluator. */
typedef union ai_ability_parameters {
    struct {
        u8 range;
        u8 aoe;
        u8 item_id;
        u8 status_infliction[STATUS_BYTE_COUNT];
    } fields;
    struct {
        u32 _first;
        u32 status_tail;
    } packed;
} ai_ability_parameters_t;

/* Pack canonical status IDs 8..39 into the aligned 32-bit value covering status
 * bytes 1..4 of ai_ability_parameters_t. */
#define AI_STATUS_TAIL_PACKED_MASK(id) ((u32)STATUS_BYTE_MASK(id) << ((STATUS_BYTE_INDEX(id) - 1) * 8))

/* Considered ability, base+0x00. */
typedef struct ai_considered_ability {
    u8 skillset;                        /* 0x00 */
    u8 ct;                              /* 0x01 */
    s16 ability_id;                     /* 0x02; read with both lh and lhu */
    ai_ability_parameters_t parameters; /* 0x04 */
    ai_ability_flags_t ai_flags;        /* 0x0c */
    u8 element;                         /* 0x10 */
    u8 mp_cost;                         /* 0x11 */
    u8 _padding_12[2];                  /* tail padding to 4-byte alignment */
} ai_considered_ability_t;
typedef char ai_considered_ability_size_must_be_20[(sizeof(ai_considered_ability_t) == 20) ? 1 : -1];

/* action records */
/* ai_action_rank_t.decided of the selected action once it is chosen. */
enum {
    AI_ACTION_DECIDED = 0xff,
};

/* How an action follows its target, in the AI/menu command and stored unit command. */
typedef enum battle_targeting_type {
    BATTLE_TARGETING_TYPE_TILE = 5, /* aims at target_position */
    BATTLE_TARGETING_TYPE_UNIT = 6, /* follows the unit target_id */
} battle_targeting_type_e;

/* The ranking tail of an action, compared by
 * ai_compare_target_priority_and_hit_rate. */
typedef struct ai_action_rank {
    /* AI_ACTION_DECIDED on the selected action once the turn's action is
     * chosen, 0 on every candidate; the first comparison key at 0x80196db0. */
    u8 decided;
    u8 base_hit_percent;
    s16 priority;
} ai_action_rank_t;
typedef char ai_action_rank_size_must_be_4[(sizeof(ai_action_rank_t) == 4) ? 1 : -1];

/*
 * One considered/recorded action, 0x20 bytes.  The current action lives at
 * base+0x14, the best so far at +0x34, eight ranked outcomes at +0x54, the
 * inverted-priority scratch at +0x154 and the selected action at +0x17fc.
 */
typedef struct ai_action_data {
    u8 act_before_move;  /* 0x00; 1: act from the current tile, then move; 0: move to coords first */
    u8 reflected_action; /* 0x01; set while evaluating reflected-origin candidates */
    u8 _unused_02;
    u8 wait_facing_hint;      /* 0x03; direction 0..3, 4 keeps current, 5 chooses during Wait */
    ai_coords_t coords;       /* 0x04 */
    action_command_t command; /* 0x08; copied as the ACT payload */
    ai_action_rank_t rank;    /* 0x1c */
} ai_action_data_t;
typedef char ai_action_data_size_must_be_32[(sizeof(ai_action_data_t) == 32) ? 1 : -1];

/* The packed 16-bit value combines an ability ID and a physical unit slot. */
enum {
    AI_ABILITY_ENTRY_ABILITY_MASK = 0x03ff,
    AI_ABILITY_ENTRY_UNIT_SHIFT = 10,
};

/* Runtime policy byte in ai_ability_entry_t. The low three bits control
 * how the AI searches and consumes candidate action origins; initialization
 * recomputes them while preserving the remaining bits. */
typedef enum ai_ability_entry_flag {
    AI_ABILITY_ENTRY_CONSUME_EVALUATED_ORIGINS = 0x01,
    AI_ABILITY_ENTRY_FORCE_CONSUME_EVALUATED_ORIGINS = 0x02,
    AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN = 0x04,
    AI_ABILITY_ENTRY_REQUIRES_MONSTER_SKILL = 0x08,
    AI_ABILITY_ENTRY_ENEMIES_ONLY = 0x20,
    AI_ABILITY_ENTRY_ALLIES_ONLY = 0x40,
    AI_ABILITY_ENTRY_USABLE = 0x80,
    AI_ABILITY_ENTRY_INIT_PRESERVE_MASK = 0xf8,
} ai_ability_entry_flag_e;

/* Four-byte entries in each compact unit's AI ability list. Whole usage
 * bytes are copied into considered flags4; policy updates address its bits. */
typedef struct ai_ability_entry {
    s16 packed_id; /* bits 0..9: ability ID; bits 10..15: physical unit slot */
    u8 skillset;
    union {
        u8 flags;
        struct {
            u8 consume_evaluated_origins : 1;
            u8 force_consume_evaluated_origins : 1;
            u8 exhaustive_origin_scan : 1;
            u8 requires_monster_skill : 1;
            u8 _unused_00_bit4 : 1;
            u8 enemies_only : 1;
            u8 allies_only : 1;
            u8 usable : 1;
        } policy;
    } usage;
} ai_ability_entry_t;
typedef char ai_ability_entry_must_be_4[sizeof(ai_ability_entry_t) == 4 ? 1 : -1];

/* unit decision records */
/* OR-combined categories gathered from both equipped weapon formulas. They
 * are flags, not ability_formula_e IDs; mixed categories bypass the builder
 * switch's single-category cases. */
typedef enum ai_weapon_formula_flags {
    AI_WEAPON_FORMULA_NONE = 0x00, /* no weapon in either hand */
    AI_WEAPON_FORMULA_DAMAGE = 0x01,
    AI_WEAPON_FORMULA_ABSORB_HP = 0x02,
    AI_WEAPON_FORMULA_HEAL = 0x04,
} ai_weapon_formula_flags_e;

/* Per-compact-unit weapon summary. The builder at 0x8019a2f0 clears two
 * 32-bit values, then merges weapon properties through named fields. */
typedef union ai_weapon_data {
    u32 words[2];
    struct {
        u8 range;
        u8 flags_1;
        u8 flags_2;
        u8 flags_3;
        u8 weapon_id;
        u8 element;
        u8 _unused_06[2];
    } fields;
} ai_weapon_data_t;
typedef char ai_weapon_data_must_be_8[sizeof(ai_weapon_data_t) == 8 ? 1 : -1];

/* Per-unit AI behavior flags copied from ENTD data to decision +0x04. The
 * three transient status bits are rebuilt during action simulation. */
typedef enum ai_decision_flags {
    AI_DECISION_TRANSIENT_DEAD = 0x01,
    AI_DECISION_TRANSIENT_PETRIFIED = 0x02,
    AI_DECISION_TRANSIENT_JUMP = 0x04,
    AI_DECISION_SPECIAL_BEHAVIOR = 0x08,
    AI_DECISION_AGGRESSIVE = 0x10,
    AI_DECISION_STAY_NEAR_COORDINATES = 0x20,
    AI_DECISION_FOCUS_TARGET = 0x40,
    /* Worth acting on: every non-enemy and every enemy with an action charging;
     * the refresh simulation adds each enemy that ends it alive (or dead with
     * Reraise), unpetrified and without Death Sentence. Target searches skip a
     * unit without it unless the unit is confused. The copy from the ENTD
     * record leaves the stay-near tile's level here until the first refresh. */
    AI_DECISION_CONSIDERED = 0x80,
} ai_decision_flags_e;

typedef enum ai_targeting_flags_1 {
    AI_TARGETING_FLAG_1_HAS_RANGED_ABILITY = 0x01, /* a usable ability has a direct or arc trajectory */
    AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES = 0x02,    /* a usable ability affects HP and targets allies */
    AI_TARGETING_FLAG_1_MOSTLY_SUPPORT = 0x04,     /* ally-targeting share of the usable abilities >= 77/128 */
    AI_TARGETING_FLAG_1_REFLECTED_ACTION = 0x08,
} ai_targeting_flags_1_e;

/* Targeting policy copied from ENTD AI Flags 2 to decision +0x07. The low
 * policy bits survive ai_refresh_unit_decision_flags; the remaining
 * target-state bits are rebuilt there before action simulation. */
typedef enum ai_targeting_flags_2 {
    AI_TARGET_HP_BELOW_HALF = 0x01,
    AI_TARGET_MP_CONSTRAINED = 0x02,
    AI_TARGET_CONSERVE_CT = 0x04,
    AI_TARGET_TEAM_SUPPORT_UNAVAILABLE = 0x08,
    AI_TARGET_UNTARGETABLE = 0x10,
    AI_TARGET_DEAD_WITHOUT_RERAISE = 0x20,
    AI_TARGET_DEAD_WITH_RERAISE = 0x40,
    AI_TARGET_HP_BELOW_THREE_QUARTERS = 0x80,
} ai_targeting_flags_2_e;

typedef enum ai_target_setting_flags {
    AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS = 0x01000000,
    AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS = 0x02000000,
    AI_TARGET_SETTING_HAS_HOLY_WATER = 0x20000000, /* the acting unit can use the Holy Water item ability */
    AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE = 0x40000000,
} ai_target_setting_flags_e;

/*
 * Per-unit AI decision record, 0x10 bytes at base+0x182c (unit*16 + 0x182c).
 * acting_unit_decision points at the acting unit's record.
 */
typedef struct ai_unit_decision {
    ai_coords_t target;   /* 0x00..0x03; x, elevation, y, 0 */
    u8 flags;             /* 0x04; ai_decision_flags_e */
    u8 main_target_id;    /* 0x05 */
    u8 targeting_flags_1; /* 0x06; 0x08 selected reflected action; 0x04 helpful-ability ratio >= 77/128, cleared for
                             the lowest-ratio nonenemy (0x8019537c..0x80195408) */
    u8 targeting_flags_2; /* 0x07; ai_targeting_flags_2_e */
    u8 enemy_flag;        /* 0x08 */
    u8 highest_mp_cost;   /* 0x09 */
    u8 lowest_mp_cost;    /* 0x0a */
    u8 mp_ability_mod;    /* 0x0b */
    u8 silence_mod;       /* 0x0c */
    u8 evade_mod;         /* 0x0d */
    u8 enemy_ability_min_reach; /* 0x0e; least range + AoE of the enemy-targeting abilities, plus Move */
    u8 ally_ability_min_reach;  /* 0x0f; least range + AoE of the ally-targeting abilities, plus Move */
} ai_unit_decision_t;
typedef char ai_unit_decision_size_must_be_16[(sizeof(ai_unit_decision_t) == 16) ? 1 : -1];

/* workspace layout */
/* AI +0x0c78..0x0cb7. The target's 21-byte save at +0xca2 overlaps
 * the low three bytes of target_setting_flags at +0xcb4. Loops at
 * 0x8019b184..0x8019b1b8 and 0x8019b260..0x8019b274 establish the span.
 * The CONSUME_EVALUATED_ORIGINS flag lies in the fourth byte, outside the saved array.
 * Preserve both views and 32-bit alignment; this is not a 16-byte backup. */
typedef union ai_targetability {
    struct {
        u8 unit_active[BATTLE_UNIT_SLOT_COUNT];
        u8 unit_targetable[BATTLE_UNIT_SLOT_COUNT];
        u8 saved_prefix[16];
        u8 _padding_3a[2]; /* aligns target_setting_flags */
        u32 target_setting_flags;
    } live;
    struct {
        u8 _unused_00[42];
        u8 unit_targetable_saved[BATTLE_UNIT_SLOT_COUNT];
        u8 _unused_3f;
    } snapshot;
} ai_targetability_t;
typedef char ai_targetability_size_must_be_64[(sizeof(ai_targetability_t) == 64) ? 1 : -1];

typedef struct ai_data {
    ai_considered_ability_t considered_ability;              /* 0x0000 */
    ai_action_data_t current_action;                         /* 0x0014 */
    ai_action_data_t best_action;                            /* 0x0034 */
    ai_action_data_t ranked_actions[AI_RANKED_ACTION_COUNT]; /* 0x0054 */
    ai_action_data_t inverted_priority_action;               /* 0x0154 */
    u8 tile_foe_proximity[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]
                         [AI_GRID_COLUMN_COUNT]; /* 0x0174; level/y/x, strides
                                                  * 288/16/1 at 0x80196e68..0x80196ebc */
    u8 tile_target_distance[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]
                           [AI_GRID_COLUMN_COUNT]; /* 0x03b4; level/y/x, strides 288/16/1 */
    s16 tile_priority[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]
                     [AI_GRID_COLUMN_COUNT]; /* 0x05f4; level/y/x, byte strides 576/32/2 */
    /* Scenario/level/y; row bits 15..0 represent x=0..15. */
    u16 reachable_tiles[AI_MOVEMENT_SCENARIO_COUNT][MAP_TERRAIN_LAYER_COUNT]
                       [AI_GRID_ROW_COUNT];                              /* 0x0a74; byte strides 72/36/2 */
    u16 targetable_tiles[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT];    /* 0x0b4c */
    u16 movable_tiles[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT];       /* 0x0b94 */
    u16 attack_origin_tiles[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]; /* 0x0bdc */
    u16 walkable_tiles[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT];      /* 0x0c24 */
    u8 _unused_0c6c;
    u8 initial_selection_stage; /* 0x0c6d; ai_initial_selection_stage_e */
    u8 action_selection_phase;  /* 0x0c6e; status, targeted and recovery selection stages */
    u8 _unused_0c6f;
    u8 outcome_evaluation_state;        /* 0x0c70; ai_ability_evaluation_stage_e */
    u8 movement_decision_state;         /* 0x0c71; ai_movement_stage_e */
    u8 search_resume_flag;              /* 0x0c72; distance-search initialization/resume flag
                                         * at 0x801994f8; reused as a phase flag at 0x8019cd9c. */
    u8 _padding_0c73;                   /* aligns considered_unit_coords */
    ai_coords_t considered_unit_coords; /* 0x0c74 */
    ai_targetability_t targetability;   /* 0x0c78 */
    /* Included target/strike pairs, not unique units; 0xff disables counting. */
    u16 total_hit_percent;       /* 0x0cb8 */
    u8 hit_counter;              /* 0x0cba */
    u8 attack_origin_tile_count; /* 0x0cbb */
    u8 movable_tile_count;       /* 0x0cbc */
    u8 ability_counter;          /* 0x0cbd */
    u8 has_mp_recovery_ability;  /* 0x0cbe; a usable ability affects MP and does not target enemies */
    u8 tile_pickup_kind;         /* 0x0cbf; ai_tile_pickup_kind_e */
    u8 _unused_0cc0[4];
    ai_coords_t candidate_coords[AI_MOVEMENT_SCENARIO_COUNT]; /* 0x0cc4; indexed by movement_scenario */
    u8 _unused_0cd0[4];
    action_command_t unit_action_records[AI_COMPACT_UNIT_COUNT]; /* 0x0cd4 */
    u16 throw_ability_id;                                        /* 0x0e14 */
    u8 throw_weapon_id;                                          /* 0x0e16 */
    /* All slots with entd_slot != 0xff, capped at 255 by 0x80199d20. */
    u8 present_unit_average_max_hp;                 /* 0x0e17 */
    u8 unit_acts_before_me[BATTLE_UNIT_SLOT_COUNT]; /* 0x0e18..0x0e2c; all slots cleared at 0x8019d408–0x8019d41c */
    u8 movement_scenario;                           /* 0x0e2d; 0..2 */
    u8 acting_unit_id;                              /* 0x0e2e */
    u8 acting_unit_compact_index;                   /* 0x0e2f; unit_compact_indices[acting_unit_id] */
    ai_coords_t acting_unit_coords;                 /* 0x0e30 */
    ai_unit_decision_t* acting_unit_decision;       /* 0x0e34 */
    u8 acting_unit_move;                            /* 0x0e38 */
    u8 acting_unit_team;                            /* 0x0e39 */
    u8 map_max_x;                                   /* 0x0e3a */
    u8 map_max_y;                                   /* 0x0e3b */
    u16 team_golem[BATTLE_TEAM_COUNT];              /* 0x0e3c; 16-bit scoring at 0x8019d37c */
    u8 status_to_cancel[STATUS_BYTE_COUNT]; /* 0x0e44; usable actor abilities' aggregate cancellation mask, built
                                                      at 0x80195410 */
    /* Resumable reflected-origin search, 0x8019b7b8–0x8019bb20.
     * Candidate x/y = 2 * reflector position - intended target position. */
    u8 reflector_unit_index;                                         /* 0x0e49 */
    u8 reflected_target_unit_index;                                  /* 0x0e4a */
    u8 reflected_candidate_level;                                    /* 0x0e4b */
    u8 ability_effect_on_self;                                       /* 0x0e4c; 0 none, 1 usable, 2 adverse */
    u8 ranked_origin_scan;                                           /* 0x0e4d; consume the ranked origin tiles */
    u8 useful_on_caster_flag;                                        /* 0x0e4e */
    u8 acting_unit_remaining_clockticks;                             /* 0x0e4f */
    ai_status_snapshot_t unit_status_records[AI_COMPACT_UNIT_COUNT]; /* 0x0e50 */
    u8 saved_ability_ct;                                             /* 0x0ed0 */
    u8 saved_current_status;                                         /* 0x0ed1 */
    u8 saved_inflicted_status;                                       /* 0x0ed2 */
    u8 _unused_0ed3;
    u8 main_ai_state;          /* 0x0ed4; ai_command_build_stage_e */
    u8 autobattle_state;       /* 0x0ed5; ai_autobattle_stage_e */
    u8 charging_state;         /* 0x0ed6; ai_charging_stage_e */
    u8 cast_origin_stage;      /* 0x0ed7; ai_cast_origin_stage_e */
    u8 reflected_origin_phase; /* 0x0ed8; cleared at 0x8019ba40, set at 0x8019ba78 */
    u8 highest_priority_state; /* 0x0ed9; ai_origin_evaluation_stage_e */
    u8 max_possibilities;      /* 0x0eda; byte-truncated combination count, then scan x at 0x8019bf2c */
    u8 y_counter;              /* 0x0edb */
    u8 map_level_counter;      /* 0x0edc */
    /* Calculator search counters survive suspension at 0x8019bbbc–0x8019bf2c. */
    u8 math_ability_id;       /* 0x0edd; low byte of the selected ability entry */
    u8 math_type_index;       /* 0x0ede; 0..3 */
    u8 math_multiplier_index; /* 0x0edf; 4..7 */
    /* Turn-controller globals saved/restored by 0x8019d37c–0x8019db80. */
    s32 saved_between_turn_state;        /* 0x0ee0; g_battle_between_turn_state during the simulation */
    s32 saved_between_turn_resume_state; /* 0x0ee4; g_battle_between_turn_resume_state */
    s32 saved_turn_unit_id;              /* 0x0ee8; g_battle_turn_unit_id */
    u16 simulated_turn_events;           /* 0x0eec; interturn loop stops at 256 */
    u8 action_simulation_phase;          /* 0x0eee; ai_simulation_phase_e */
    u8 _padding_0eef;                    /* aligns ability_lists */
    ai_ability_entry_t ability_lists[AI_COMPACT_UNIT_COUNT][AI_ABILITY_LIST_COUNT]; /* 0x0ef0; 0x88 bytes per unit */
    /* Blood Suck/Frog IDs start without unit bits; 0x8019729c/0x80197510
     * OR the acting physical slot into bits 10..15 before loading the entry. */
    ai_ability_entry_t hardcoded_status_abilities[2];         /* 0x1770 */
    ai_weapon_data_t unit_weapon_data[AI_COMPACT_UNIT_COUNT]; /* 0x1778 */
    unit_stats_t* acting_unit;                                /* 0x17f8 */
    ai_action_data_t selected_action;                         /* 0x17fc */
    /* Triggered trap tiles are excluded from movement choices unless
     * the acting unit has Confusion or Berserk. */
    ai_coords_t known_trap_coords[AI_KNOWN_TRAP_COUNT];        /* 0x181c; x == 0xff terminates the list */
    ai_unit_decision_t unit_decisions[BATTLE_UNIT_SLOT_COUNT]; /* 0x182c */
    /* 0x80195ed4 saves the complete decision before retreat; 0x80195efc
     * restores it only after the child finishes, not when it suspends. */
    ai_unit_decision_t saved_acting_unit_decision;    /* 0x197c */
    u8 unit_compact_indices[BATTLE_UNIT_SLOT_COUNT];  /* 0x198c; slot -> index among present units (0..15) */
    u8 unit_autobattle_modes[BATTLE_UNIT_SLOT_COUNT]; /* 0x19a1; ai_autobattle_mode_e from ENTD AI flags */
    u8 autobattle_setting;                            /* 0x19b6 */
    u8 resume_pending;                                /* 0x19b7; an AI routine suspended on its frame budget */
    u8 random_use_threshold;                          /* 0x19b8; 0x80195778..0x80195788 stores
                                                       * floor(128 * random-use entries / eligible entries).
                                                       * A zero denominator leaves the prior byte unchanged. */
    u8 water_penalty;                                 /* 0x19b9 */
    u8 valuable_target_hit;                           /* 0x19ba; a counted hit on unit_targetable sets this
                                                       * at 0x8019e010..0x8019e020. */
    u8 single_enemy_remaining;                        /* 0x19bb; exactly one enemy counted */
    u8 ability_processing_done;                       /* 0x19bc */
    u8 action_taken;                                  /* 0x19bd */
    u8 movement_taken;                                /* 0x19be */
    u8 _unused_19bf[8];
    u8 _padding_19c7; /* 0x19c7; last documented byte; tail padding to 4-byte alignment */
} ai_data_t;
typedef char ai_data_size_must_be_6600[(sizeof(ai_data_t) == 6600) ? 1 : -1];

/* workspace */
/* BATTLE-resident workspace; EVENT, EFFECT and WORLD overlays also use its pointer. */
extern ai_data_t g_battle_ai_data_base;
extern ai_data_t* g_ai_workspace;        /* 0x8019f3ac */
extern event_work_t* g_ai_workspace_ptr; /* pointer cell reloaded per subsystem */

void battle_ai_init_workspace(void);

/* commands */
typedef enum ai_command_kind {
    AI_COMMAND_MOVE = 0,
    AI_COMMAND_ACT = 1,
    AI_COMMAND_END_TURN = 2,
    /* The rest reach the command from menu_resolve_selection, for a player's menu choice.
     * The auto-battle choices are 12 plus twice the option, plus one once a unit has been
     * picked for the two that name one (SELECT picks it, DISPLAY shows it); SELECT and the two
     * others equal their ai_autobattle_mode_e values. */
    AI_COMMAND_NO_MENU_RECORD = 7, /* the current menu id has no record: back to the turn events */
    AI_COMMAND_AUTOBATTLE_TARGETED_SELECT = 12,
    AI_COMMAND_AUTOBATTLE_TARGETED_DISPLAY = 13,
    AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT = 14,
    AI_COMMAND_AUTOBATTLE_RECOVERY_DISPLAY = 15,
    AI_COMMAND_AUTOBATTLE_FADING_LIFE = 16,
    AI_COMMAND_AUTOBATTLE_RETREAT = 17,
    AI_COMMAND_REFRESH_CONTROL = 18, /* copy the control flag back from the stats */
    AI_COMMAND_MOVE_SELF = 25,       /* MOVE that never redirects to the unit's mount */
    AI_COMMAND_UNRESOLVED_FE = 0xfe,
    AI_COMMAND_UNRESOLVED_FF = 0xff,
} ai_command_kind_e;

/* The setup dispatcher writes this at runtime +0x158 and copies 24 bytes into
 * each history slot. ACT copies the 20-byte payload from AI action +0x08;
 * MOVE writes only x/layer/y, and END_TURN leaves the payload unchanged. */
typedef struct ai_command {
    ai_command_kind_e kind;
    union {
        map_tile_coordinates_t move;
        action_command_t action;
    } data;
} ai_command_t;
typedef char ai_command_size_must_be_24[sizeof(ai_command_t) == 24 ? 1 : -1];

typedef enum ai_command_build_stage {
    AI_COMMAND_BUILD_REFRESH = 0,
    AI_COMMAND_BUILD_MOVEMENT = 1,
    AI_COMMAND_BUILD_CHOOSE_ACTION = 2,
} ai_command_build_stage_e;

extern ai_command_t g_battle_ai_command_history[2]; /* 0x801a0d94, 0x801a0dac */

s32 battle_ai_build_command(s32 unit_id, ai_command_t* command);

/* action selection */
/* Byte-sized resume labels for initial action selection; the gaps are not dispatched. */
typedef enum ai_initial_selection_stage {
    AI_INITIAL_SELECTION_STATUS = 0,
    AI_INITIAL_SELECTION_RETREAT = 2,
    AI_INITIAL_SELECTION_AUTOBATTLE = 3,
    AI_INITIAL_SELECTION_GENERAL = 7,
} ai_initial_selection_stage_e;

typedef enum ai_status_selection_stage {
    AI_STATUS_SELECTION_CHICKEN = 0,
    AI_STATUS_SELECTION_TARGET_DISTANCE = 1,
} ai_status_selection_stage_e;

/* These three suspend points use their own byte; other values resume abilities. */
typedef enum ai_autobattle_stage {
    AI_AUTOBATTLE_CHARGING = 2,
    AI_AUTOBATTLE_MOVEMENT = 4,
    AI_AUTOBATTLE_ABILITIES = 7,
} ai_autobattle_stage_e;

typedef enum ai_targeted_selection_stage {
    AI_TARGETED_SELECTION_SCORE_TILES = 0,
    AI_TARGETED_SELECTION_MOVEMENT = 1,
    AI_TARGETED_SELECTION_CHARGING = 2,
    AI_TARGETED_SELECTION_TARGET_ABILITIES = 3,
    AI_TARGETED_SELECTION_OTHER_ABILITIES = 4,
} ai_targeted_selection_stage_e;

/* No pass saves stage 5; it follows the same ally-search fallback as stage 6. */
typedef enum ai_recovery_selection_stage {
    AI_RECOVERY_SELECTION_SCORE_TILES = 0,
    AI_RECOVERY_SELECTION_MOVEMENT = 1,
    AI_RECOVERY_SELECTION_CHARGING = 2,
    AI_RECOVERY_SELECTION_TARGET_ABILITIES = 3,
    AI_RECOVERY_SELECTION_ENEMY_ABILITIES = 4,
    AI_RECOVERY_SELECTION_ALLY_ABILITIES = 6,
    AI_RECOVERY_SELECTION_CANCEL_STATUS = 7,
} ai_recovery_selection_stage_e;

typedef enum ai_retreat_stage {
    AI_RETREAT_MOVEMENT = 0,
    AI_RETREAT_SELF_ABILITIES = 1,
    AI_RETREAT_CHARGING = 2,
    AI_RETREAT_OTHER_ABILITIES = 3,
} ai_retreat_stage_e;

enum {
    AI_RECOVERY_STATUS_COUNT = 3,
};

extern u8 g_battle_ai_recovery_status_ids[AI_RECOVERY_STATUS_COUNT];
extern s32 g_battle_ai_recovery_status_index; /* 0x8019f380; status-list cursor preserved for resume */
extern s32 g_battle_ai_recovery_status_retry; /* 0x8019f37c; skip status pass after one failed attempt */
extern s32 g_battle_ai_retreat_stage;         /* 0x8019f390; ai_retreat_stage_e */

/* ability lists */
void battle_ai_init_unit_abilities(s32 unit_id);
s32 battle_ai_is_ability_known(s32 unit_id, s32 skillset_id, s32 bit);

/* target selection */
typedef enum ai_unit_filter {
    AI_UNIT_FILTER_ANY = 0,
    AI_UNIT_FILTER_ENEMIES = 1,
    AI_UNIT_FILTER_ALLIES = 2,
    AI_UNIT_FILTER_SPECIFIC = 3,
    AI_UNIT_FILTER_STATUS = 4,
} ai_unit_filter_e;

typedef enum ai_nearest_target_mode {
    AI_NEAREST_WITHOUT_BLOOD_SUCK = 0,
    AI_NEAREST_IMMINENT_ENEMY = 1,
    AI_NEAREST_ENEMY = 2,
    AI_NEAREST_UNIT = 3,
    AI_NEAREST_ELIGIBLE_ENEMY = 4,
    AI_NEAREST_HEALER_OR_CRYSTAL = 5,
} ai_nearest_target_mode_e;

/* Classification from the acting unit's side, not the sign of HP damage.
 * A useful effect may include deliberately hitting an ally to affect status. */
typedef enum ai_ability_effect {
    AI_ABILITY_EFFECT_NONE = 0,
    AI_ABILITY_EFFECT_USEFUL = 1,
    AI_ABILITY_EFFECT_ADVERSE = 2,
} ai_ability_effect_e;

ai_ability_effect_e battle_ai_classify_ability_effect(s32 unit_id, s32 check_reflect);
s32 battle_ai_find_nearest_target(ai_nearest_target_mode_e mode);
s32 battle_ai_find_unit_at_coordinates(const ai_coords_t* coords);
s32 battle_ai_is_unit_untargetable(s32 unit_id);

/* status and turn timing */
s32 battle_ai_calculate_clockticks_until_death_counter_expires();
s32 battle_ai_calculate_clockticks_until_unit_acts(const unit_stats_t* unit);
s32 battle_ai_is_status_active_through_delay(s32 delay, const unit_stats_t* unit, s32 status_id);

/* movement */
typedef enum ai_movement_stage {
    AI_MOVEMENT_TARGET_DISTANCE = 0,
    AI_MOVEMENT_SCORE_TILES = 1,
} ai_movement_stage_e;

typedef enum ai_cowardly_movement_stage {
    AI_COWARDLY_MOVEMENT_SCORE_TILES = 0,
    AI_COWARDLY_MOVEMENT_TARGET_DISTANCE = 1,
} ai_cowardly_movement_stage_e;

typedef enum ai_destination_order {
    AI_DESTINATION_PRIORITY_FIRST = 0,
    AI_DESTINATION_DISTANCE_FIRST = 1,
} ai_destination_order_e;

extern ai_cowardly_movement_stage_e g_battle_ai_cowardly_movement_stage;
extern s32 g_battle_ai_propagation_resume_pass;

s32 battle_ai_is_pathfinding_budget_exhausted(void);
s32 battle_ai_propagate_target_movement(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget);
void battle_ai_record_triggered_trap_coords(s32 x, s32 y, s32 level);

/* target geometry */
typedef enum ai_tile_mask_mode {
    AI_TILE_MASK_RANGE = 0,
    AI_TILE_MASK_AOE = 1,
    AI_TILE_MASK_RANGE_PLUS_AOE = 2,
    AI_TILE_MASK_EXPLICIT = 3,
} ai_tile_mask_mode_e;

extern u16 g_battle_ai_range_row_masks[];

void battle_ai_build_attack_origin_masks(void);
s32 battle_ai_calculate_distance_between_units(const unit_stats_t* a, const unit_stats_t* b);
s32 battle_ai_calculate_height_difference_between_units(const ai_coords_t* coords, const unit_stats_t* unit);
void battle_ai_copy_unit_coordinates(s32 unit_index, ai_coords_t* destination);
s32 battle_ai_count_flagged_tiles_within_map_bounds(u16 rows[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]);
s32 battle_ai_is_ability_origin_allowed(const ai_coords_t* coordinates);

/* ability evaluation */
typedef enum ai_ability_evaluation_stage {
    AI_ABILITY_EVALUATION_REFLECTED_TARGETS = 0,
    AI_ABILITY_EVALUATION_SELF_TARGET_RANGE = 1,
    AI_ABILITY_EVALUATION_UNIT_TARGETS = 2,
    AI_ABILITY_EVALUATION_FIRST_CT_GROUP = 3,
    AI_ABILITY_EVALUATION_SECOND_CT_GROUP = 4,
    AI_ABILITY_EVALUATION_TILE_TARGETS = 5,
    AI_ABILITY_EVALUATION_MATH_TARGETS = 6,
    AI_ABILITY_EVALUATION_PREPARE = 7,
    AI_ABILITY_EVALUATION_MAP_TARGETS = 8,
} ai_ability_evaluation_stage_e;

typedef enum ai_origin_evaluation_stage {
    AI_ORIGIN_EVALUATION_PREPARE = 0,
    AI_ORIGIN_EVALUATION_RANKED = 1,
    AI_ORIGIN_EVALUATION_SCANNED = 2,
} ai_origin_evaluation_stage_e;

typedef enum ai_cast_origin_stage {
    AI_CAST_ORIGIN_ACT_BEFORE_MOVE = 0,
    AI_CAST_ORIGIN_MOVE_BEFORE_ACT = 1,
} ai_cast_origin_stage_e;

typedef enum ai_charging_stage {
    AI_CHARGING_SONG_DANCE = 0,
    AI_CHARGING_WITH_CHARGE = 1,
    AI_CHARGING_WITHOUT_CHARGE = 2,
} ai_charging_stage_e;

extern s32 g_battle_ai_self_target_origin_stage;

s32 battle_ai_evaluate_ability_outcome(void);
s32 battle_ai_evaluate_cast_origins(void);
s32 battle_ai_evaluate_current_action(void);

/* math skill */
extern s32 g_battle_ai_math_ability_index;
extern u16* g_battle_ai_math_ability_list; /* borrowed from main's temporary skillset list */
extern u8 g_battle_ai_math_extra_known_ability_ids[AI_MATH_EXTRA_KNOWN_ABILITY_COUNT];
extern s32 g_battle_ai_math_skillset_id;
extern s32 g_battle_ai_math_skillset_index;
extern u8 g_battle_ai_math_source_skillsets[AI_MATH_SOURCE_SKILLSET_COUNT];

/* status cancellation */
/* Cancellation-search accounting survives suspension. */
extern s32 g_battle_ai_status_candidate_count;
extern s32 g_battle_ai_status_insufficient_mp;
extern s32 g_battle_ai_status_sufficient_mp;

/* action ranking */
typedef enum ai_rank_order {
    AI_RANK_BELOW = 0,
    AI_RANK_EQUAL = 1,
    AI_RANK_ABOVE = 2,
} ai_rank_order_e;

void battle_ai_init_selected_action(void);
void battle_ai_insert_ranked_action(void);
s32 battle_ai_is_action_higher_ranked(const ai_action_data_t* a, const ai_action_data_t* b);
void battle_ai_reset_action_ranking_fields(ai_action_data_t* action);

/* simulation */
/* Baseline records include action/equipment data; simulated status uses the compact records. */
typedef enum ai_snapshot_mode {
    AI_SNAPSHOT_BASELINE = 0,
    AI_SNAPSHOT_SIMULATED_STATUS = 1,
} ai_snapshot_mode_e;

typedef enum ai_simulation_phase {
    AI_SIMULATION_PREPARE_ACTION = 0,
    AI_SIMULATION_ADVANCE_TURNS = 1,
    AI_SIMULATION_SCORE_STATE = 2,
} ai_simulation_phase_e;

/* Pickup occupant on the current action tile; treasure takes precedence. */
typedef enum ai_tile_pickup_kind {
    AI_TILE_PICKUP_NONE = 0,
    AI_TILE_PICKUP_CRYSTAL = 1,
    AI_TILE_PICKUP_TREASURE = 2,
} ai_tile_pickup_kind_e;

/* Unit snapshots share the scratchpad-save arena used during movement propagation. */
extern ai_extended_snapshot_t g_battle_ai_unit_snapshot_storage[];
extern ai_extended_snapshot_t* g_battle_ai_unit_snapshots; /* 0x8019f3c0 */

void battle_ai_begin_action_simulation(void);
void battle_ai_end_action_simulation(void);
void battle_ai_move_temp_unit_to_coords(const ai_coords_t* coordinates);
s32 battle_ai_simulate_action_and_score(void);
void battle_ai_update_tile_pickup(void);

/* facing */
/* Only the four cardinal hints index the tile-delta table. */
enum {
    AI_CARDINAL_DIRECTION_COUNT = 4,
};

typedef enum ai_direction {
    AI_DIRECTION_SOUTH = 0,
    AI_DIRECTION_WEST = 1,
    AI_DIRECTION_NORTH = 2,
    AI_DIRECTION_EAST = 3,
    AI_DIRECTION_OVERLAP = 4,
    AI_DIRECTION_CHOOSE_AT_WAIT = 5,
} ai_direction_e;

/* Cardinal map-tile deltas stored as wrapping unsigned bytes. */
typedef struct ai_facing_tile_offsets {
    u8 by_direction[AI_CARDINAL_DIRECTION_COUNT][2];
} ai_facing_tile_offsets_t;

extern ai_facing_tile_offsets_t g_battle_ai_facing_tile_offsets; /* 0x8019f358 */

s32 battle_ai_choose_wait_facing(void);
ai_direction_e battle_ai_find_direction_of_target(const ai_coords_t* target, const ai_coords_t* origin);

/* arithmetic */

#endif
