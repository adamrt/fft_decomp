#ifndef AI_H
#define AI_H

/* The battle AI's workspace (ai_data_t) and the records it is built from, with the per-unit ability lists and the AI's
 * memory copy helpers. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/battle_map.h"
#include "fft/script.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/types.h"

/* workspace capacities */
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

/* coordinates */
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
 * low bit means CONSUME_EVALUATED_AIM_TILES, not the static EVADE_WITH_MOTION.
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
    AI_ABILITY_CONSUME_EVALUATED_AIM_TILES = 0x01000000,
    AI_ABILITY_EXHAUSTIVE_ORIGIN_SCAN = 0x04000000,
    AI_ABILITY_REQUIRES_MONSTER_SKILL = 0x08000000,
    AI_ABILITY_ENEMIES_ONLY = 0x20000000,
    AI_ABILITY_ALLIES_ONLY = 0x40000000,
    AI_ABILITY_USABLE_BY_AI = 0x80000000,
};

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

/* considered ability */
/* AI ability +0x04..0x0b: byte parameters and an aligned 32-bit status tail.
 * ai_target_classify_ability_effect loads status bytes 1..4 together; reading only one byte
 * would silently lose the high status bits tested by the decision evaluator. */
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

/* The ability being considered (ai_data_t.considered_ability). */
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

/* actions */
/* ai_action_rank_t.decided of the selected action once it is chosen. */
enum {
    AI_ACTION_DECIDED = 0xff,
};

/* The ranking tail of an action, compared by
 * ai_rank_compare. */
typedef struct ai_action_rank {
    /* AI_ACTION_DECIDED on the selected action once the turn's action is
     * chosen, 0 on every candidate; the first comparison key. */
    u8 decided;
    u8 base_hit_percent;
    s16 priority;
} ai_action_rank_t;
typedef char ai_action_rank_size_must_be_4[(sizeof(ai_action_rank_t) == 4) ? 1 : -1];

/* One considered or recorded action. ai_data_t holds the current one, the best so far, eight
 * ranked outcomes, the move-only baseline and the selected one. */
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

/* ability lists */
/* The packed 16-bit value combines an ability ID and a physical unit slot. */
enum {
    AI_ABILITY_ENTRY_ABILITY_MASK = 0x03ff,
    AI_ABILITY_ENTRY_UNIT_SHIFT = 10,
};

/* Runtime policy byte in ai_ability_entry_t. The low three bits control
 * how the AI searches action origins and consumes evaluated aim tiles; initialization
 * recomputes them while preserving the remaining bits. */
typedef enum ai_ability_entry_flag {
    AI_ABILITY_ENTRY_CONSUME_EVALUATED_AIM_TILES = 0x01,
    AI_ABILITY_ENTRY_FORCE_CONSUME_EVALUATED_AIM_TILES = 0x02,
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
            u8 consume_evaluated_aim_tiles : 1;
            u8 force_consume_evaluated_aim_tiles : 1;
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

void ai_init_unit_abilities(s32 unit_id);
s32 ai_ability_is_known(s32 unit_id, s32 skillset_id, s32 bit);

/* unit records */
/* Shared prefix of the baseline and suspended AI unit snapshots.
 * Byte 2 holds Death Sentence CT only in the compact suspended record;
 * the baseline uses status_ct[15] and leaves this byte untouched. */
typedef struct ai_status_snapshot {
    u8 entd_slot;
    u8 death_counter;
    u8 death_sentence_ct;
    u8 current_status[STATUS_BYTE_COUNT];
} ai_status_snapshot_t;
typedef char ai_status_snapshot_must_be_8[sizeof(ai_status_snapshot_t) == 8 ? 1 : -1];

/* OR-combined categories gathered from both equipped weapon formulas. They
 * are flags, not ability_formula_e IDs; mixed categories bypass the builder
 * switch's single-category cases. */
typedef enum ai_weapon_formula_flags {
    AI_WEAPON_FORMULA_NONE = 0x00, /* no weapon in either hand */
    AI_WEAPON_FORMULA_DAMAGE = 0x01,
    AI_WEAPON_FORMULA_ABSORB_HP = 0x02,
    AI_WEAPON_FORMULA_HEAL = 0x04,
} ai_weapon_formula_flags_e;

/* Per-compact-unit weapon summary. ai_ability_build_list clears two
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

/* unit decisions */
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

/* Shares of usable abilities are measured in 128ths. */
enum {
    AI_ABILITY_SHARE_SCALE = 128,
    AI_SUPPORT_SHARE_THRESHOLD = 77,
};

typedef enum ai_targeting_flags_1 {
    AI_TARGETING_FLAG_1_HAS_RANGED_ABILITY = 0x01, /* a usable ability has a direct or arc trajectory */
    AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES = 0x02,    /* a usable ability affects HP and targets allies */
    AI_TARGETING_FLAG_1_MOSTLY_SUPPORT = 0x04,     /* ally-targeting share of the usable abilities >= 77/128 */
    AI_TARGETING_FLAG_1_REFLECTED_ACTION = 0x08,
} ai_targeting_flags_1_e;

/* Targeting policy copied from ENTD AI Flags 2 to decision +0x07. The low
 * policy bits survive ai_decision_refresh_flags; the remaining
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

/* Per-unit AI decision record (ai_data_t.unit_decisions); acting_unit_decision points at the
 * acting unit's. */
typedef struct ai_unit_decision {
    ai_coords_t target;         /* 0x00..0x03; x, elevation, y, 0 */
    u8 flags;                   /* 0x04; ai_decision_flags_e */
    u8 main_target_id;          /* 0x05 */
    u8 targeting_flags_1;       /* 0x06; ai_targeting_flags_1_e. MOSTLY_SUPPORT is cleared again for the
                                   nonenemy with the lowest share (ai_ability_set_considerations) */
    u8 targeting_flags_2;       /* 0x07; ai_targeting_flags_2_e */
    u8 enemy_flag;              /* 0x08 */
    u8 highest_mp_cost;         /* 0x09 */
    u8 lowest_mp_cost;          /* 0x0a */
    u8 mp_ability_mod;          /* 0x0b */
    u8 silence_mod;             /* 0x0c */
    u8 evade_mod;               /* 0x0d */
    u8 enemy_ability_min_reach; /* 0x0e; least range + AoE of the enemy-targeting abilities, plus Move */
    u8 ally_ability_min_reach;  /* 0x0f; least range + AoE of the ally-targeting abilities, plus Move */
} ai_unit_decision_t;
typedef char ai_unit_decision_size_must_be_16[(sizeof(ai_unit_decision_t) == 16) ? 1 : -1];

/* workspace */
typedef enum ai_target_setting_flags {
    AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS = 0x01000000,
    AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES = 0x02000000,
    AI_TARGET_SETTING_HAS_HOLY_WATER = 0x20000000, /* the acting unit can use the Holy Water item ability */
    AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE = 0x40000000,
} ai_target_setting_flags_e;

/* ai_data_t.targetability, +0x0c78..0x0cb7. The 21-byte save of unit_targetable at +0xca2
 * overlaps the low three bytes of target_setting_flags at +0xcb4; the save and restore loops in
 * ai_ability_evaluate_outcome establish the span. The CONSUME_EVALUATED_AIM_TILES flag lies
 * in the fourth byte, outside the saved array. Preserve both views and 32-bit alignment; this is
 * not a 16-byte backup. */
typedef union ai_targetability {
    struct {
        u8 unit_active[UNIT_SLOT_COUNT];
        u8 unit_targetable[UNIT_SLOT_COUNT];
        u8 saved_prefix[16];
        u8 _padding_3a[2]; /* aligns target_setting_flags */
        u32 target_setting_flags;
    } live;
    struct {
        u8 _unused_00[42];
        u8 unit_targetable_saved[UNIT_SLOT_COUNT];
        u8 _unused_3f;
    } snapshot;
} ai_targetability_t;
typedef char ai_targetability_size_must_be_64[(sizeof(ai_targetability_t) == 64) ? 1 : -1];

/* The whole AI state, kept across the frames a decision is suspended over. The stage bytes hold
 * the resume points of the AI's passes (ai_command.h, ai_decision.h). */
typedef struct ai_data {
    ai_considered_ability_t considered_ability;              /* 0x0000 */
    ai_action_data_t current_action;                         /* 0x0014 */
    ai_action_data_t best_action;                            /* 0x0034 */
    ai_action_data_t ranked_actions[AI_RANKED_ACTION_COUNT]; /* 0x0054 */
    ai_action_data_t move_baseline_action;                   /* 0x0154 */
    u8 tile_foe_proximity[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]
                         [AI_GRID_COLUMN_COUNT]; /* 0x0174; level/y/x */
    u8 tile_target_distance[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]
                           [AI_GRID_COLUMN_COUNT];                                              /* 0x03b4; level/y/x */
    s16 tile_priority[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT][AI_GRID_COLUMN_COUNT]; /* 0x05f4; level/y/x */
    /* Scenario/level/y; row bits 15..0 represent x=0..15. */
    u16 reachable_tiles[AI_MOVEMENT_SCENARIO_COUNT][BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]; /* 0x0a74 */
    u16 targetable_tiles[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT];                            /* 0x0b4c */
    u16 cast_origin_tiles[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT];                           /* 0x0b94 */
    u16 aim_tiles[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT];                                   /* 0x0bdc */
    u16 walkable_tiles[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT];                              /* 0x0c24 */
    u8 _unused_0c6c;
    u8 initial_selection_stage; /* 0x0c6d; ai_initial_selection_stage_e */
    u8 action_selection_phase;  /* 0x0c6e; status, targeted and recovery selection stages */
    u8 _unused_0c6f;
    u8 outcome_evaluation_state;        /* 0x0c70; ai_ability_evaluation_stage_e */
    u8 movement_decision_state;         /* 0x0c71; ai_movement_stage_e */
    u8 search_resume_flag;              /* 0x0c72; initialize/resume flag of ai_move_fill_target_distance_grid;
                                         * ai_origin_evaluate_aim_tiles reuses it as a phase flag */
    u8 _padding_0c73;                   /* aligns considered_unit_coords */
    ai_coords_t considered_unit_coords; /* 0x0c74 */
    ai_targetability_t targetability;   /* 0x0c78 */
    /* Included target/strike pairs, not unique units; 0xff disables counting. */
    u16 total_hit_percent;      /* 0x0cb8 */
    u8 hit_counter;             /* 0x0cba */
    u8 aim_tile_count;          /* 0x0cbb */
    u8 cast_origin_tile_count;  /* 0x0cbc */
    u8 ability_counter;         /* 0x0cbd */
    u8 has_mp_recovery_ability; /* 0x0cbe; a usable ability affects MP and does not target enemies */
    u8 tile_pickup_kind;        /* 0x0cbf; ai_tile_pickup_kind_e */
    u8 _unused_0cc0[4];
    ai_coords_t candidate_coords[AI_MOVEMENT_SCENARIO_COUNT]; /* 0x0cc4; indexed by movement_scenario */
    u8 _unused_0cd0[4];
    action_command_t unit_action_records[AI_COMPACT_UNIT_COUNT]; /* 0x0cd4 */
    u16 throw_ability_id;                                        /* 0x0e14 */
    u8 throw_weapon_id;                                          /* 0x0e16 */
    /* All slots with entd_slot != 0xff, capped at 255 by ai_init_workspace. */
    u8 present_unit_average_max_hp;           /* 0x0e17 */
    u8 unit_acts_before_me[UNIT_SLOT_COUNT];  /* 0x0e18; all slots cleared by the action simulation */
    u8 movement_scenario;                     /* 0x0e2d; 0..2 */
    u8 acting_unit_id;                        /* 0x0e2e */
    u8 acting_unit_compact_index;             /* 0x0e2f; unit_compact_indices[acting_unit_id] */
    ai_coords_t acting_unit_coords;           /* 0x0e30 */
    ai_unit_decision_t* acting_unit_decision; /* 0x0e34 */
    u8 acting_unit_move;                      /* 0x0e38 */
    u8 acting_unit_team;                      /* 0x0e39 */
    u8 map_max_x;                             /* 0x0e3a */
    u8 map_max_y;                             /* 0x0e3b */
    u16 team_golem[UNIT_TEAM_COUNT];          /* 0x0e3c; scored as 16-bit values by the action simulation */
    u8 status_to_cancel[STATUS_BYTE_COUNT];   /* 0x0e44; the statuses the actor's usable abilities cancel, built by
                                                        ai_ability_set_considerations */
    /* Resumable reflected-origin search (ai_origin_evaluate_reflected).
     * Candidate x/y = 2 * reflector position - intended target position. */
    u8 reflector_unit_index;                                         /* 0x0e49 */
    u8 reflected_target_unit_index;                                  /* 0x0e4a */
    u8 reflected_candidate_level;                                    /* 0x0e4b */
    u8 ability_effect_on_self;                                       /* 0x0e4c; 0 none, 1 usable, 2 adverse */
    u8 ranked_origin_scan;                                           /* 0x0e4d; consume the ranked origin tiles */
    u8 consume_aim_tile;                                             /* 0x0e4e */
    u8 acting_unit_remaining_clockticks;                             /* 0x0e4f */
    ai_status_snapshot_t unit_status_records[AI_COMPACT_UNIT_COUNT]; /* 0x0e50 */
    u8 saved_ability_ct;                                             /* 0x0ed0 */
    u8 saved_current_status;                                         /* 0x0ed1 */
    u8 saved_inflicted_status;                                       /* 0x0ed2 */
    u8 _unused_0ed3;
    u8 main_ai_state;          /* 0x0ed4; ai_command_build_stage_e */
    u8 autobattle_state;       /* 0x0ed5; ai_general_stage_e */
    u8 charging_state;         /* 0x0ed6; ai_charging_stage_e */
    u8 cast_origin_stage;      /* 0x0ed7; ai_cast_origin_stage_e */
    u8 reflected_origin_phase; /* 0x0ed8; set and cleared by the reflected-origin search */
    u8 highest_priority_state; /* 0x0ed9; ai_origin_evaluation_stage_e */
    u8 max_possibilities;      /* 0x0eda; byte-truncated combination count, then the scan x of
                                * ai_origin_evaluate_reachable */
    u8 y_counter;              /* 0x0edb */
    u8 map_level_counter;      /* 0x0edc */
    /* Calculator search counters (ai_math_evaluate_targets) survive suspension. */
    u8 math_ability_id;       /* 0x0edd; low byte of the selected ability entry */
    u8 math_type_index;       /* 0x0ede; 0..3 */
    u8 math_multiplier_index; /* 0x0edf; 4..7 */
    /* Turn-controller globals saved and restored around the action simulation. */
    s32 saved_between_turn_state;        /* 0x0ee0; g_turn_clock_state during the simulation */
    s32 saved_between_turn_resume_state; /* 0x0ee4; g_turn_clock_resume_state */
    s32 saved_turn_unit_id;              /* 0x0ee8; g_turn_unit_id */
    u16 simulated_turn_events;           /* 0x0eec; interturn loop stops at 256 */
    u8 action_simulation_phase;          /* 0x0eee; ai_simulation_phase_e */
    u8 _padding_0eef;                    /* aligns ability_lists */
    ai_ability_entry_t ability_lists[AI_COMPACT_UNIT_COUNT][AI_ABILITY_LIST_COUNT]; /* 0x0ef0; 0x88 bytes per unit */
    /* Blood Suck/Frog IDs start without unit bits; ai_select_status_imposed ORs the
     * acting physical slot into bits 10..15 before loading the entry. */
    ai_ability_entry_t hardcoded_status_abilities[2];         /* 0x1770 */
    ai_weapon_data_t unit_weapon_data[AI_COMPACT_UNIT_COUNT]; /* 0x1778 */
    unit_stats_t* acting_unit;                                /* 0x17f8 */
    ai_action_data_t selected_action;                         /* 0x17fc */
    /* Triggered trap tiles are excluded from movement choices unless
     * the acting unit has Confusion or Berserk. */
    ai_coords_t known_trap_coords[AI_KNOWN_TRAP_COUNT]; /* 0x181c; x == 0xff terminates the list */
    ai_unit_decision_t unit_decisions[UNIT_SLOT_COUNT]; /* 0x182c */
    /* ai_select_action saves the complete decision before retreat and restores
     * it only after the retreat pass finishes, not when it suspends. */
    ai_unit_decision_t saved_acting_unit_decision; /* 0x197c */
    u8 unit_compact_indices[UNIT_SLOT_COUNT];      /* 0x198c; slot -> index among present units (0..15) */
    u8 unit_autobattle_modes[UNIT_SLOT_COUNT];     /* 0x19a1; ai_autobattle_mode_e from ENTD AI flags */
    u8 autobattle_setting;                         /* 0x19b6 */
    u8 resume_pending;                             /* 0x19b7; an AI routine suspended on its frame budget */
    u8 random_use_threshold;                       /* 0x19b8; floor(128 * random-use entries / eligible
                                                    * entries), from ai_ability_set_considerations.
                                                    * A zero denominator leaves the prior byte unchanged. */
    u8 water_penalty;                              /* 0x19b9 */
    u8 valuable_target_hit;                        /* 0x19ba; set by ai_simulate_strikes for a
                                                    * counted hit on a unit_targetable unit */
    u8 single_enemy_remaining;                     /* 0x19bb; exactly one enemy counted */
    u8 ability_processing_done;                    /* 0x19bc */
    u8 action_taken;                               /* 0x19bd */
    u8 movement_taken;                             /* 0x19be */
    u8 _unused_19bf[8];
    u8 _padding_19c7; /* 0x19c7; last documented byte; tail padding to 4-byte alignment */
} ai_data_t;
typedef char ai_data_size_must_be_6600[(sizeof(ai_data_t) == 6600) ? 1 : -1];

/* The workspace lives in BATTLE; EVENT, EFFECT and WORLD code also use its pointer.
 * ai_init_workspace points g_ai_workspace at g_ai_data, and the menus borrow
 * the same memory through g_ai_workspace_ptr as their work block. */
extern ai_data_t g_ai_data;
extern ai_data_t* g_ai_workspace;
extern event_work_t* g_ai_workspace_ptr; /* pointer cell reloaded per subsystem */

void ai_init_workspace(void);

/* simulation */
/* The copy of g_action_mimic_attacker that ai_simulate_begin saves and ai_simulate_end restores. */
extern action_mimic_attacker_t g_ai_mimic_attacker_backup;

/* memory helpers */
/* Copy forward using a signed destination-end address; nonpositive counts do nothing. */
void ai_mem_copy_bytes(void* destination, const void* source, s32 byte_count);

/* Copy byte_count / 2 unsigned 16-bit values; leave an odd final byte unchanged. */
void ai_mem_copy_u16_values(void* destination, const void* source, s32 byte_count);

/* Clear byte_count / 4 signed 32-bit values; leave any trailing bytes unchanged. */
void ai_mem_zero_32bit_values(void* destination, s32 byte_count);

#endif
