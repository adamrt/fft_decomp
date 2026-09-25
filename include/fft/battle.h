#ifndef FFT_BATTLE_H
#define FFT_BATTLE_H

/* BATTLE.BIN: the battle engine. Event and effect overlays run alongside it. */

#include "fft/main.h"
#include "psx/libc.h"

struct battle_event_input_state;
union battle_texture_prim;

/* ability */
typedef enum battle_ability_facing_modifier {
    BATTLE_ABILITY_FACING_FRONT = 0,
    BATTLE_ABILITY_FACING_SIDE = 1,
    BATTLE_ABILITY_FACING_BACK = 2,
} battle_ability_facing_modifier_e;

typedef enum battle_secondary_effect_phase {
    BATTLE_SECONDARY_EFFECT_INITIALIZING = 1,
    BATTLE_SECONDARY_EFFECT_EXECUTING = 2,
    BATTLE_SECONDARY_EFFECT_FINALIZING = 3
} battle_effect_secondary_phase_e;

typedef struct battle_ability_animation_data {
    u8 charge_animation_set_id;
    u8 attack_animation;
    u8 text_display; /* External table evidence; no reconstructed consumer yet. */
} battle_ability_animation_data_t;
typedef char battle_ability_animation_data_size_must_be_3[(sizeof(battle_ability_animation_data_t) == 3) ? 1 : -1];

enum {
    BATTLE_ABILITY_ANIMATION_COUNT = 0x1c6,
    BATTLE_CHARGE_ANIMATION_SET_COUNT = 20,
    BATTLE_CHARGE_ANIMATION_VARIANT_COUNT = 2,
    BATTLE_ATTACK_ANIMATION_USE_WEAPON = 0,
    BATTLE_ATTACK_ANIMATION_USE_ITEM = 1,
};

typedef enum battle_support_set_2 {
    BATTLE_SUPPORT_SET_2_HALF_MP = 0x80,
    BATTLE_SUPPORT_SET_2_GAINED_JP_UP = 0x40,
    BATTLE_SUPPORT_SET_2_GAINED_EXP_UP = 0x20,
    BATTLE_SUPPORT_SET_2_ATTACK_UP = 0x10,
    BATTLE_SUPPORT_SET_2_DEFENSE_UP = 0x08,
    BATTLE_SUPPORT_SET_2_MAGIC_ATTACK_UP = 0x04,
    BATTLE_SUPPORT_SET_2_MAGIC_DEFENSE_UP = 0x02,
    BATTLE_SUPPORT_SET_2_CONCENTRATE = 0x01,
} battle_unit_support_set_2_e;

typedef enum battle_support_set_4 {
    BATTLE_SUPPORT_SET_4_MONSTER_SKILL = 0x80,
    BATTLE_SUPPORT_SET_4_DEFEND = 0x40,
    BATTLE_SUPPORT_SET_4_EQUIP_CHANGE = 0x20,
    BATTLE_SUPPORT_SET_4_SHORT_CHARGE = 0x08,
    BATTLE_SUPPORT_SET_4_NON_CHARGE = 0x04,
} battle_unit_support_set_4_e;

/* Primary ability-record byte 3 at 0x8005ebf0. Learn-on-hit is consumed at
 * 0x8018e6b8; WORLD 0x80122b08 and JOBSTTS 0x801e03bc prove the JP-learning
 * flag's negative polarity. */
typedef enum ability_type_flag {
    ABILITY_TYPE_FLAG_LEARN_ON_HIT = 0x20,
    ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP = 0x80,
} ability_type_flag_e;

enum {
    ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT = 7,
};

typedef enum ability_type {
    ABILITY_TYPE_DEFAULT = 0,
    ABILITY_TYPE_ITEM = 1,
    ABILITY_TYPE_THROW = 2,
    ABILITY_TYPE_JUMP = 3,
    ABILITY_TYPE_CHARGE = 4,
    ABILITY_TYPE_MATH = 5,
    ABILITY_TYPE_REACTION = 6,
    ABILITY_TYPE_SUPPORT = 7,
    ABILITY_TYPE_MOVEMENT = 8,
} ability_type_e;

/* Behavior flags in the four secondary bytes of a default ability record.
 * Only meanings confirmed by reconstructed target consumers are named here. */
typedef enum ability_secondary_flags_1 {
    ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF = 0x01,
    ABILITY_SECONDARY_FLAG_1_AUTO = 0x02,
    ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE = 0x04,
    ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE = 0x20,
} ability_secondary_flags_1_e;

typedef enum ability_secondary_flags_2 {
    ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER = 0x01,
    ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE = 0x08,
    ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES = 0x40,
    ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES = 0x80,
} ability_secondary_flags_2_e;

typedef enum ability_secondary_flags_4 {
    ABILITY_SECONDARY_FLAG_4_EVADEABLE = 0x02,
    ABILITY_SECONDARY_FLAG_4_REQUIRES_MATERIA_BLADE = 0x04,
    ABILITY_SECONDARY_FLAG_4_REQUIRES_SWORD = 0x08,
    ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE = 0x10,
    ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING = 0x20,
    ABILITY_SECONDARY_FLAG_4_COUNTER_MAGIC = 0x40,
    ABILITY_SECONDARY_FLAG_4_COUNTER_FLOOD = 0x80,
} ability_secondary_flags_4_e;

extern battle_ability_animation_data_t g_battle_ability_animation_data[BATTLE_ABILITY_ANIMATION_COUNT];

extern u8 g_battle_ability_charge_animation_sets[BATTLE_CHARGE_ANIMATION_SET_COUNT]
                                                [BATTLE_CHARGE_ANIMATION_VARIANT_COUNT];

extern s16 g_ability_effect_id;
extern s32 g_selected_ability;
extern s32 g_battle_spell_quote_last_ability_id;
extern u8 g_battle_loaded_ability_flags_1;

/* Ability-list window layout at 0x80166994. Column modes: 0 text, 1 number,
 * 2 hidden, 3 alternate number. The columns read the work buffer's u16 text
 * ids and values through the layout's s16 pointers. */
extern world_menu_text_layout_t g_battle_ability_menu_layout;
extern u8 g_battle_spell_quote_exception_skillsets[];
s32 battle_ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id);

/* ai */
typedef enum battle_ai_command_kind {
    BATTLE_AI_COMMAND_MOVE = 0,
    BATTLE_AI_COMMAND_ACT = 1,
    BATTLE_AI_COMMAND_END_TURN = 2,
} battle_ai_command_kind_e;

enum {
    BATTLE_AI_ABILITY_FLAG_1_TARGET_ENEMIES = 0x02,
    BATTLE_AI_ABILITY_FLAG_1_TARGET_ALLIES = 0x01,
    BATTLE_AI_ABILITY_FLAG_2_TARGET_MAP_TILES = 0x80,
    BATTLE_AI_ABILITY_FLAG_3_EXHAUSTIVE_ORIGIN_MASK = 0x3c,
    BATTLE_AI_ABILITY_FLAG_4_USABLE_BY_AI = 0x80,
};

/* Full-word masks, distinct from the byte-view FLAG_1 constants above.
 * Tests in 0x8019e5d8..0x8019ef24 establish the low-byte effect categories
 * and distinguish team preferences from the absolute ONLY restrictions. */
enum {
    BATTLE_AI_ABILITY_TARGET_ALLIES = 0x00000001,
    BATTLE_AI_ABILITY_TARGET_ENEMIES = 0x00000002,
    BATTLE_AI_ABILITY_UNEQUIP = 0x00000004,
    BATTLE_AI_ABILITY_AFFECTS_STATS = 0x00000008,
    BATTLE_AI_ABILITY_ADD_STATUS = 0x00000010,
    BATTLE_AI_ABILITY_CANCEL_STATUS = 0x00000020,
    BATTLE_AI_ABILITY_AFFECTS_MP = 0x00000040,
    BATTLE_AI_ABILITY_AFFECTS_HP = 0x00000080,
    BATTLE_AI_ABILITY_AFFECTED_BY_SILENCE = 0x00000100,
    BATTLE_AI_ABILITY_EVADEABLE = 0x00000200,
    BATTLE_AI_ABILITY_AFFECTED_BY_FAITH = 0x00000400,
    BATTLE_AI_ABILITY_RANDOM_USE = 0x00000800,
    BATTLE_AI_ABILITY_FOLLOW_TARGET = 0x00001000,
    BATTLE_AI_ABILITY_UNDEAD_REVERSE = 0x00002000,
    BATTLE_AI_ABILITY_REFLECTABLE = 0x00004000,
    BATTLE_AI_ABILITY_TARGET_MAP_TILES = 0x00008000,
    BATTLE_AI_ABILITY_LINEAR_TRAJECTORY = 0x00200000,
    BATTLE_AI_ABILITY_DIRECT_TRAJECTORY = 0x00400000,
    BATTLE_AI_ABILITY_ARC_TRAJECTORY = 0x00800000,
    BATTLE_AI_ABILITY_CONSUME_EVALUATED_ORIGINS = 0x01000000,
    BATTLE_AI_ABILITY_EXHAUSTIVE_ORIGIN_SCAN = 0x04000000,
    BATTLE_AI_ABILITY_REQUIRES_MONSTER_SKILL = 0x08000000,
    BATTLE_AI_ABILITY_ENEMIES_ONLY = 0x20000000,
    BATTLE_AI_ABILITY_ALLIES_ONLY = 0x40000000,
    BATTLE_AI_ABILITY_USABLE_BY_AI = 0x80000000,
};

typedef struct battle_ai_command_action {
    u8 unit_id;
    u8 skillset;
    u16 ability_id;
    u16 calculator_type;
    u16 calculator_multiplier;
    u8 item_id;
    u8 _unused_09;
    u8 targeting_type;
    u8 target_id;
    u16 target_x;
    u16 target_elevation;
    u16 target_y;
    u8 _unused_12[2];
} battle_ai_command_action_t;
typedef char battle_ai_command_action_size_must_be_20[sizeof(battle_ai_command_action_t) == 20 ? 1 : -1];

/* The setup dispatcher writes this at Misc +0x158 and copies 24 bytes into
 * each history slot. ACT copies the 20-byte payload from AI action +0x08;
 * MOVE writes only x/elevation/y, and END_TURN leaves the payload unchanged. */
typedef struct battle_ai_command {
    battle_ai_command_kind_e kind;
    union {
        u16 halfwords[10];
        struct {
            u16 x;
            u16 elevation;
            u16 y;
            u8 _unused_06[14];
        } move;
        battle_ai_command_action_t action;
    } data;
} battle_ai_command_t;
typedef char battle_ai_command_size_must_be_24[sizeof(battle_ai_command_t) == 24 ? 1 : -1];

/*
 * AI data block, 0x19c8 bytes at 0x8019f3c4 (g_battle_ai_data_base).
 * Unidentified ranges stay explicit padding.  Scalar aliases such as
 * g_battle_ai_acting_unit_decision_ptr name the same bytes.  Sites spell fields
 * as members; an alias remains only where the member spelling lets GCC derive
 * the address from another address in this block (one shared base register)
 * while the target reloads %hi/%lo at that site.
 */
/* X / map elevation / Y / zero, written in that order by transfer_unit_coordinates_to_ai
 * and read as one word for equality tests. */
typedef union battle_ai_coords {
    u32 word;
    struct {
        u8 x;
        u8 elevation;
        u8 y;
        u8 zero;
    } bytes;
} battle_ai_coords_t;

/* Ability AI behaviour flags 1..4. Consumers read both the full word and its
 * constituent bytes; use the corresponding masks above for each view. */
typedef union battle_ai_ability_flags {
    u32 word;
    struct {
        u8 flags_1;
        u8 flags_2;
        u8 flags_3;
        u8 flags_4;
    } bytes;
} battle_ai_ability_flags_t;

/* Shared prefix of the baseline and suspended AI unit snapshots.
 * Byte 2 holds Death Sentence CT only in the compact suspended record;
 * the baseline uses status_ct[15] and leaves this byte untouched. */
typedef struct battle_ai_status_snapshot {
    u8 entd_slot;
    u8 death_counter;
    u8 death_sentence_ct; /* see 0x8019e214 */
    u8 current_status[5];
} battle_ai_status_snapshot_t;
typedef char battle_ai_status_snapshot_must_be_8[sizeof(battle_ai_status_snapshot_t) == 8 ? 1 : -1];

/* Selected unit state saved at 0x8019e160 and restored at 0x8019e378.
 * HP occupies unaligned bytes 0x1b..0x1c; max HP/MP are not saved. */
typedef struct battle_ai_extended_snapshot {
    battle_ai_status_snapshot_t status; /* 0x00 */
    u8 status_ct[16];                   /* 0x08 */
    u8 team_flags;                      /* 0x18 */
    u8 faith;                           /* 0x19 */
    u8 transparent_removal_flag;        /* 0x1a */
    u8 hp_bytes[2];                     /* 0x1b */
    u8 _padding_1d;                     /* aligns mp */
    u16 mp;                             /* 0x1e */
    u8 charged_ability_ct;              /* 0x20 */
    u8 base_attributes[3];              /* 0x21; unit_attribute_index_e */
    u8 attributes[3];                   /* 0x24; unit_attribute_index_e */
    u8 ct;                              /* 0x27 */
    u8 x;                               /* 0x28 */
    u8 position_bytes[2];               /* 0x29; packed unit position */
    u8 has_turn;                        /* 0x2b */
    u8 movement_taken;
    u8 action_taken;
    u8 auto_battle_setting; /* 0x2e */
    u8 auto_battle_target;
    u8 initial_team_flags;
    u8 inflicted_status[5]; /* 0x31 */
    u8 equipment[7];        /* 0x36 */
    u8 brave;               /* 0x3d */
    u8 mount_info;
    u8 _padding_3f; /* tail padding to 2-byte alignment */
} battle_ai_extended_snapshot_t;
typedef char battle_ai_extended_snapshot_must_be_64[sizeof(battle_ai_extended_snapshot_t) == 64 ? 1 : -1];
typedef char battle_ai_snapshot_hp_must_be_0x1b[((unsigned long)&((battle_ai_extended_snapshot_t*)0)->hp_bytes == 0x1b)
        ? 1
        : -1];
typedef char battle_ai_snapshot_equipment_must_be_0x36
    [((unsigned long)&((battle_ai_extended_snapshot_t*)0)->equipment == 0x36) ? 1 : -1];

/* AI ability +0x04..0x0b: byte parameters and an aligned status-tail word.
 * 0x8019e9f4 loads status bytes 1..4 together; reading only one byte would
 * silently lose the high status bits tested by the decision evaluator. */
typedef union battle_ai_ability_parameters {
    struct {
        u8 range;
        u8 aoe;
        u8 item_id;
        u8 status_infliction[5];
    } bytes;
    struct {
        u32 _first;
        u32 status_tail;
    } words;
} battle_ai_ability_parameters_t;

/* Pack canonical status IDs 8..39 into the aligned word covering status
 * bytes 1..4 of battle_ai_ability_parameters_t. */
#define BATTLE_AI_STATUS_TAIL_PACKED_MASK(id)                                                                          \
    ((u32)BATTLE_STATUS_BYTE_MASK(id) << ((BATTLE_STATUS_BYTE_INDEX(id) - 1) * 8))

/* Considered ability, base+0x00. */
typedef struct battle_ai_considered_ability {
    u8 skillset;                               /* 0x00 */
    u8 ct;                                     /* 0x01 */
    s16 ability_id;                            /* 0x02; read with both lh and lhu */
    battle_ai_ability_parameters_t parameters; /* 0x04 */
    battle_ai_ability_flags_t ai_flags;        /* 0x0c */
    u8 element;                                /* 0x10 */
    u8 mp_cost;                                /* 0x11 */
    u8 _padding_12[2];                         /* tail padding to 4-byte alignment */
} battle_ai_considered_ability_t;
typedef char battle_ai_considered_ability_size_must_be_0x14[(sizeof(battle_ai_considered_ability_t) == 0x14) ? 1 : -1];
typedef char battle_ai_status_tail_must_be_at_offset_8
    [((unsigned long)&((battle_ai_considered_ability_t*)0)->parameters.words.status_tail == 8) ? 1 : -1];

/*
 * One considered/recorded action, 0x20 bytes.  The current action lives at
 * base+0x14, the best so far at +0x34, eight ranked outcomes at +0x54, the
 * inverted-priority scratch at +0x154 and the selected action at +0x17fc.
 */
typedef struct battle_ai_action_data {
    u8 target_flags_set; /* 0x00; set to 1; name provisional */
    u8 reflected_action; /* 0x01; set while evaluating reflected-origin candidates */
    u8 _unused_02;
    u8 wait_facing_hint;       /* 0x03; direction 0..3, 4 keeps current, 5 chooses during Wait */
    battle_ai_coords_t coords; /* 0x04 */
    u8 unit_id;                /* 0x08 */
    u8 skillset;               /* 0x09 */
    u16 ability_id;            /* 0x0a; stored with sh */
    u16 calculator_type;       /* 0x0c */
    u16 calculator_multiplier; /* 0x0e */
    u8 item_id;                /* 0x10 */
    u8 _unused_11;
    u8 targeting_type;    /* 0x12; 5 tile-targeted, 6 unit-targeted */
    u8 target_id;         /* 0x13 */
    u16 target_x;         /* 0x14; read with lhu */
    u16 target_elevation; /* 0x16; 0 or 1 map layer */
    u16 target_y;         /* 0x18 */
    u8 _unused_1a[2];
    u8 rank_byte;        /* 0x1c; first comparison key at 0x80196db0; meaning unresolved. */
    u8 base_hit_percent; /* 0x1d */
    u16 priority;        /* 0x1e */
} battle_ai_action_data_t;
typedef char battle_ai_action_data_size_must_be_0x20[(sizeof(battle_ai_action_data_t) == 0x20) ? 1 : -1];

/* unit_t AI ability list entry, 4 bytes, 0x22 per unit. */
typedef struct battle_ai_ability_entry {
    union {
        u16 packed_id; /* bits 0..9: ability ID; bits 10..15: physical unit slot */
        struct {
            u8 low;
            u8 unit_and_high;
        } bytes;
    } id;
    /* The AI updates the flag bits with lhu/sh over both bytes, so the
     * halfword view covers skillset and usage_flags together. */
    union {
        u16 packed; /* skillset | usage_flags << 8 */
        struct {
            u8 skillset;    /* 0x02; 0xff terminates the list (chose_move_from_move_list) */
            u8 usage_flags; /* 0x03; battle_ai_ability_entry_flag_e */
        } bytes;
    } skillset_flags;
} battle_ai_ability_entry_t;
typedef char battle_ai_ability_entry_must_be_4[sizeof(battle_ai_ability_entry_t) == 4 ? 1 : -1];

/* Per-compact-unit weapon summary. The builder at 0x8019a2f0 clears two
 * words, then merges weapon properties through the byte view. */
typedef union battle_ai_weapon_data {
    u32 words[2];
    struct {
        u8 range;
        u8 flags_1;
        u8 flags_2;
        u8 flags_3;
        u8 weapon_id;
        u8 element;
        u8 _unused_06[2];
    } bytes;
} battle_ai_weapon_data_t;
typedef char battle_ai_weapon_data_must_be_8[sizeof(battle_ai_weapon_data_t) == 8 ? 1 : -1];

/* Per-unit AI behavior flags copied from ENTD data to decision +0x04. The
 * three transient status bits are rebuilt during action simulation. Bit 0x80
 * is deliberately unnamed: refresh and simulation give it overlapping uses. */
typedef enum battle_ai_decision_flags {
    BATTLE_AI_DECISION_TRANSIENT_DEAD = 0x01,
    BATTLE_AI_DECISION_TRANSIENT_PETRIFIED = 0x02,
    BATTLE_AI_DECISION_TRANSIENT_JUMP = 0x04,
    BATTLE_AI_DECISION_SPECIAL_BEHAVIOR = 0x08,
    BATTLE_AI_DECISION_AGGRESSIVE = 0x10,
    BATTLE_AI_DECISION_STAY_NEAR_COORDINATES = 0x20,
    BATTLE_AI_DECISION_FOCUS_TARGET = 0x40,
} battle_ai_decision_flags_e;

/* Targeting policy copied from ENTD AI Flags 2 to decision +0x07. The low
 * policy bits survive battle_ai_refresh_unit_decision_flags; the remaining
 * target-state bits are rebuilt there before action simulation. */
typedef enum battle_ai_targeting_flags_2 {
    BATTLE_AI_TARGET_HP_BELOW_HALF = 0x01,
    BATTLE_AI_TARGET_MP_CONSTRAINED = 0x02,
    BATTLE_AI_TARGET_CONSERVE_CT = 0x04,
    BATTLE_AI_TARGET_TEAM_SUPPORT_UNAVAILABLE = 0x08,
    BATTLE_AI_TARGET_UNTARGETABLE = 0x10,
    BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE = 0x20,
    BATTLE_AI_TARGET_DEAD_WITH_RERAISE = 0x40,
    BATTLE_AI_TARGET_HP_BELOW_THREE_QUARTERS = 0x80,
} battle_ai_targeting_flags_2_e;

typedef enum battle_ai_target_setting_flags {
    BATTLE_AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS = 0x01000000,
    BATTLE_AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS = 0x02000000,
    BATTLE_AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE = 0x40000000,
} battle_ai_target_setting_flags_e;

/*
 * Per-unit AI decision record, 0x10 bytes at base+0x182c (unit*16 + 0x182c).
 * g_battle_ai_acting_unit_decision_ptr points at the acting unit's record.
 */
typedef struct battle_ai_unit_decision {
    battle_ai_coords_t target; /* 0x00..0x03; x, level, y, 0 */
    u8 flags;                  /* 0x04; battle_ai_decision_flags_e */
    u8 main_target_id;         /* 0x05 */
    u8 targeting_flags_1; /* 0x06; 0x08 selected reflected action; 0x04 helpful-ability ratio >= 77/128, cleared for
                             the lowest-ratio nonenemy (0x8019537c..0x80195408) */
    u8 targeting_flags_2; /* 0x07; battle_ai_targeting_flags_2_e */
    u8 enemy_flag;        /* 0x08 */
    u8 highest_mp_cost;   /* 0x09 */
    u8 lowest_mp_cost;    /* 0x0a */
    u8 mp_ability_mod;    /* 0x0b */
    u8 silence_mod;       /* 0x0c */
    u8 evade_mod;         /* 0x0d */
    u8 lowest_range;      /* 0x0e; plus unit move */
    u8 highest_range;     /* 0x0f; plus unit move */
} battle_ai_unit_decision_t;
typedef char battle_ai_unit_decision_size_must_be_0x10[(sizeof(battle_ai_unit_decision_t) == 0x10) ? 1 : -1];

/* AI +0x0c78..0x0cb7. The target's 21-byte save at +0xca2 overlaps
 * the low three bytes of target_setting_flags at +0xcb4. Loops at
 * 0x8019b184..0x8019b1b8 and 0x8019b260..0x8019b274 establish the span.
 * The CONSUME_EVALUATED_ORIGINS flag lies in the fourth byte, outside the saved array.
 * Preserve both views and word alignment; this is not a 16-byte backup. */
typedef union battle_ai_targetability {
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
} battle_ai_targetability_t;
typedef char battle_ai_targetability_size_must_be_64[(sizeof(battle_ai_targetability_t) == 64) ? 1 : -1];

typedef struct battle_ai_data {
    battle_ai_considered_ability_t considered_ability; /* 0x0000 */
    battle_ai_action_data_t current_action;            /* 0x0014 */
    battle_ai_action_data_t best_action;               /* 0x0034 */
    battle_ai_action_data_t ranked_actions[8];         /* 0x0054 */
    battle_ai_action_data_t inverted_priority_action;  /* 0x0154 */
    u8 tile_foe_proximity[2][18][16];                  /* 0x0174; level/y/x, strides
                                                        * 288/16/1 at 0x80196e68..0x80196ebc */
    u8 tile_target_distance[2][18][16];                /* 0x03b4; level/y/x, strides 288/16/1 */
    u16 tile_priority[2][18][16];                      /* 0x05f4; level/y/x, byte strides 576/32/2 */
    /* Scenario/level/y; row bits 15..0 represent x=0..15. */
    u16 reachable_tiles[3][2][18]; /* 0x0a74; byte strides 72/36/2 */
    u16 targetable_tiles[0x24];    /* 0x0b4c */
    u16 movable_tiles[0x24];       /* 0x0b94 */
    u16 attack_origin_tiles[0x24]; /* 0x0bdc */
    u16 walkable_tiles[0x24];      /* 0x0c24 */
    u8 _unused_0c6c;
    u8 initial_targeting_state; /* 0x0c6d */
    u8 action_selection_phase;  /* 0x0c6e; 0x801971b8 selects scorer/distance resume; 0x80197ff4 uses 0..4 */
    u8 _unused_0c6f;
    u8 outcome_evaluation_state;               /* 0x0c70 */
    u8 movement_decision_state;                /* 0x0c71 */
    u8 search_resume_flag;                     /* 0x0c72; distance-search initialization/resume flag
                                                * at 0x801994f8; reused as a phase flag at 0x8019cd9c. */
    u8 _padding_0c73;                          /* aligns considered_unit_coords */
    battle_ai_coords_t considered_unit_coords; /* 0x0c74 */
    battle_ai_targetability_t targetability;   /* 0x0c78 */
    /* Included target/strike pairs, not unique units; 0xff disables counting. */
    u16 total_hit_percent;            /* 0x0cb8 */
    u8 hit_counter;                   /* 0x0cba */
    u8 attack_origin_tile_count;      /* 0x0cbb */
    u8 movable_tile_count;            /* 0x0cbc */
    u8 ability_counter;               /* 0x0cbd */
    u8 ability_targets_enemies_or_mp; /* 0x0cbe; provisional name: any usable ability has (flags & 0x42) == 0x40 */
    u8 crystal_treasure_status;       /* 0x0cbf; 1 crystal, 2 treasure */
    u8 _unused_0cc0[4];
    battle_ai_coords_t candidate_coords[3]; /* 0x0cc4; indexed by movement_scenario */
    u8 _unused_0cd0[4];
    u8 unit_action_records[16][0x14]; /* 0x0cd4 */
    u16 throw_ability_id;             /* 0x0e14 */
    u8 throw_weapon_id;               /* 0x0e16 */
    /* All slots with entd_slot != 0xff, capped at 255 by 0x80199d20. */
    u8 present_unit_average_max_hp;                  /* 0x0e17 */
    u8 unit_acts_before_me[BATTLE_UNIT_SLOT_COUNT];  /* 0x0e18..0x0e2c; all slots cleared at 0x8019d408–0x8019d41c */
    u8 movement_scenario;                            /* 0x0e2d; 0..2 */
    u8 acting_unit_id;                               /* 0x0e2e */
    u8 acting_unit_battle_id;                        /* 0x0e2f */
    battle_ai_coords_t acting_unit_coords;           /* 0x0e30 */
    battle_ai_unit_decision_t* acting_unit_decision; /* 0x0e34 */
    u8 acting_unit_move;                             /* 0x0e38 */
    u8 acting_unit_team;                             /* 0x0e39 */
    u8 map_max_x;                                    /* 0x0e3a */
    u8 map_max_y;                                    /* 0x0e3b */
    union {
        u8 bytes[8];
        u16 by_team[4];
    } team_golem;           /* 0x0e3c; halfword scoring at 0x8019d37c */
    u8 status_to_cancel[5]; /* 0x0e44; usable actor abilities' aggregate cancellation mask, built at 0x80195410 */
    /* Resumable reflected-origin search, 0x8019b7b8–0x8019bb20.
     * Candidate x/y = 2 * reflector position - intended target position. */
    u8 reflector_unit_index;             /* 0x0e49 */
    u8 reflected_target_unit_index;      /* 0x0e4a */
    u8 reflected_candidate_level;        /* 0x0e4b */
    u8 ability_effect_on_self;           /* 0x0e4c; 0 none, 1 usable, 2 adverse */
    u8 weapon_range_flag;                /* 0x0e4d */
    u8 useful_on_caster_flag;            /* 0x0e4e */
    u8 acting_unit_remaining_clockticks; /* 0x0e4f */
    u8 unit_status_records[16][8];       /* 0x0e50 */
    u8 saved_ability_ct;                 /* 0x0ed0 */
    u8 saved_current_status;             /* 0x0ed1 */
    u8 saved_inflicted_status;           /* 0x0ed2 */
    u8 _unused_0ed3;
    u8 main_ai_state;             /* 0x0ed4 */
    u8 autobattle_state;          /* 0x0ed5 */
    u8 charging_state;            /* 0x0ed6 */
    u8 find_highest_target_state; /* 0x0ed7; check_if_map_allows_use_and_find_highest_target's progress variable */
    u8 reflected_origin_phase;    /* 0x0ed8; cleared at 0x8019ba40, set at 0x8019ba78 */
    u8 highest_priority_state;    /* 0x0ed9 */
    u8 max_possibilities;         /* 0x0eda; byte-truncated combination count, then scan x at 0x8019bf2c */
    u8 y_counter;                 /* 0x0edb */
    u8 map_level_counter;         /* 0x0edc */
    /* Calculator search counters survive suspension at 0x8019bbbc–0x8019bf2c. */
    u8 math_ability_id;       /* 0x0edd; low byte of the selected ability entry */
    u8 math_type_index;       /* 0x0ede; 0..3 */
    u8 math_multiplier_index; /* 0x0edf; 4..7 */
    /* Turn-controller globals saved/restored by 0x8019d37c–0x8019db80. */
    s32 saved_turn_state_0;                            /* 0x0ee0; snapshot of 0x8018f518 */
    s32 saved_turn_state_1;                            /* 0x0ee4; snapshot of 0x8018f51c */
    s32 saved_turn_state_2;                            /* 0x0ee8; snapshot of 0x8018f520 */
    u16 simulated_turn_events;                         /* 0x0eec; interturn loop stops at 256 */
    u8 progress_0eee;                                  /* 0x0eee */
    u8 _padding_0eef;                                  /* aligns ability_lists */
    battle_ai_ability_entry_t ability_lists[16][0x22]; /* 0x0ef0; 0x88 bytes per unit */
    /* Blood Suck/Frog IDs start without unit bits; 0x8019729c/0x80197510
     * OR the acting physical slot into bits 10..15 before loading the entry. */
    battle_ai_ability_entry_t hardcoded_status_abilities[2]; /* 0x1770 */
    battle_ai_weapon_data_t unit_weapon_data[16];            /* 0x1778 */
    battle_stats_t* acting_unit;                             /* 0x17f8 */
    battle_ai_action_data_t selected_action;                 /* 0x17fc */
    /* 0x8019aea0 compares/copies coordinate words; 0x80194da4 consumes
     * x/elevation/y to clear reachable tiles. The list's broader policy is unknown. */
    battle_ai_coords_t coords_181c[4];                                /* 0x181c; x == 0xff terminates the list */
    battle_ai_unit_decision_t unit_decisions[BATTLE_UNIT_SLOT_COUNT]; /* 0x182c */
    /* 0x80195ed4 saves the complete decision before retreat; 0x80195efc
     * restores it only after the child finishes, not when it suspends. */
    battle_ai_unit_decision_t saved_acting_unit_decision; /* 0x197c */
    u8 unit_battle_ids[BATTLE_UNIT_SLOT_COUNT];           /* 0x198c */
    u8 unit_behaviour[BATTLE_UNIT_SLOT_COUNT];            /* 0x19a1; 0x11 coward, 0x0e/0x0c aggressive */
    u8 autobattle_setting;                                /* 0x19b6 */
    u8 decision_state;                                    /* 0x19b7 */
    u8 random_use_threshold;                              /* 0x19b8; 0x80195778..0x80195788 stores
                                                           * floor(128 * random-use entries / eligible entries).
                                                           * A zero denominator leaves the prior byte unchanged. */
    u8 water_penalty;                                     /* 0x19b9 */
    u8 valuable_target_hit;                               /* 0x19ba; a counted hit on unit_targetable sets this
                                                           * at 0x8019e010..0x8019e020. */
    u8 targetable_override;                               /* 0x19bb */
    u8 ability_processing_done;                           /* 0x19bc */
    u8 action_taken;                                      /* 0x19bd */
    u8 movement_taken;                                    /* 0x19be */
    u8 _unused_19bf[8];
    u8 _padding_19c7; /* 0x19c7; last documented byte; tail padding to 4-byte alignment */
} battle_ai_data_t;
typedef char battle_ai_data_size_must_be_0x19c8[(sizeof(battle_ai_data_t) == 0x19c8) ? 1 : -1];
typedef char battle_ai_saved_targetability_must_be_0xca2
    [((unsigned long)&((battle_ai_data_t*)0)->targetability.snapshot.unit_targetable_saved == 0xca2) ? 1 : -1];
typedef char battle_ai_target_flags_must_be_0xcb4
    [((unsigned long)&((battle_ai_data_t*)0)->targetability.live.target_setting_flags == 0xcb4) ? 1 : -1];

/* Runtime policy byte in battle_ai_ability_entry_t. The low three bits control
 * how the AI searches and consumes candidate action origins; initialization
 * recomputes them while preserving the remaining bits. */
typedef enum battle_ai_ability_entry_flag {
    BATTLE_AI_ABILITY_ENTRY_CONSUME_EVALUATED_ORIGINS = 0x01,
    BATTLE_AI_ABILITY_ENTRY_FORCE_CONSUME_EVALUATED_ORIGINS = 0x02,
    BATTLE_AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN = 0x04,
    BATTLE_AI_ABILITY_ENTRY_REQUIRES_MONSTER_SKILL = 0x08,
    BATTLE_AI_ABILITY_ENTRY_ENEMIES_ONLY = 0x20,
    BATTLE_AI_ABILITY_ENTRY_ALLIES_ONLY = 0x40,
    BATTLE_AI_ABILITY_ENTRY_USABLE = 0x80,
    BATTLE_AI_ABILITY_ENTRY_INIT_PRESERVE_MASK = 0xf8,
} battle_ai_ability_entry_flag_e;

typedef enum battle_ai_rank_order {
    BATTLE_AI_RANK_BELOW = 0,
    BATTLE_AI_RANK_EQUAL = 1,
    BATTLE_AI_RANK_ABOVE = 2,
} battle_ai_rank_order_e;

typedef enum battle_ai_unit_filter {
    BATTLE_AI_UNIT_FILTER_ANY = 0,
    BATTLE_AI_UNIT_FILTER_ENEMIES = 1,
    BATTLE_AI_UNIT_FILTER_ALLIES = 2,
    BATTLE_AI_UNIT_FILTER_SPECIFIC = 3,
    BATTLE_AI_UNIT_FILTER_STATUS = 4,
} battle_ai_unit_filter_e;

typedef enum battle_ai_nearest_target_mode {
    BATTLE_AI_NEAREST_WITHOUT_BLOOD_SUCK = 0,
    BATTLE_AI_NEAREST_IMMINENT_ENEMY = 1,
    BATTLE_AI_NEAREST_ENEMY = 2,
    BATTLE_AI_NEAREST_UNIT = 3,
    BATTLE_AI_NEAREST_ELIGIBLE_ENEMY = 4,
    BATTLE_AI_NEAREST_HEALER_OR_CRYSTAL = 5,
} battle_ai_nearest_target_mode_e;

typedef enum battle_ai_destination_order {
    BATTLE_AI_DESTINATION_PRIORITY_FIRST = 0,
    BATTLE_AI_DESTINATION_DISTANCE_FIRST = 1,
} battle_ai_destination_order_e;

typedef enum battle_ai_tile_mask_mode {
    BATTLE_AI_TILE_MASK_RANGE = 0,
    BATTLE_AI_TILE_MASK_AOE = 1,
    BATTLE_AI_TILE_MASK_RANGE_PLUS_AOE = 2,
    BATTLE_AI_TILE_MASK_EXPLICIT = 3,
} battle_ai_tile_mask_mode_e;

typedef enum battle_ai_autobattle_mode {
    BATTLE_AI_AUTOBATTLE_TARGETED = 0x0C,
    BATTLE_AI_AUTOBATTLE_RECOVERY = 0x0E,
    BATTLE_AI_AUTOBATTLE_FADING_LIFE = 0x10,
    BATTLE_AI_AUTOBATTLE_RETREAT = 0x11,
} battle_ai_autobattle_mode_e;

typedef enum battle_ai_command_build_stage {
    BATTLE_AI_COMMAND_BUILD_REFRESH = 0,
    BATTLE_AI_COMMAND_BUILD_MOVEMENT = 1,
    BATTLE_AI_COMMAND_BUILD_CHOOSE_ACTION = 2,
} battle_ai_command_build_stage_e;

typedef enum battle_ai_direction {
    BATTLE_AI_DIRECTION_SOUTH = 0,
    BATTLE_AI_DIRECTION_WEST = 1,
    BATTLE_AI_DIRECTION_NORTH = 2,
    BATTLE_AI_DIRECTION_EAST = 3,
    BATTLE_AI_DIRECTION_OVERLAP = 4,
    BATTLE_AI_DIRECTION_CHOOSE_AT_WAIT = 5,
} battle_ai_direction_e;

typedef enum battle_ai_targeting_flags_1 {
    BATTLE_AI_TARGETING_FLAG_1_REFLECTED_ACTION = 0x08,
} battle_ai_targeting_flags_1_e;

/* Classification from the acting unit's side, not the sign of HP damage.
 * A useful effect may include deliberately hitting an ally to affect status. */
typedef enum battle_ai_ability_effect {
    BATTLE_AI_ABILITY_EFFECT_NONE = 0,
    BATTLE_AI_ABILITY_EFFECT_USEFUL = 1,
    BATTLE_AI_ABILITY_EFFECT_ADVERSE = 2,
} battle_ai_ability_effect_e;

/* Signed comparison view of an action's four-byte tail at +0x1c.
 * Callers pass &action->rank_byte; the stored priority's unsigned view
 * elsewhere must not change these signed comparisons. */
typedef struct battle_ai_action_rank {
    u8 rank_byte;
    u8 base_hit_percent;
    s16 priority;
} battle_ai_action_rank_t;
typedef char battle_ai_action_rank_size_must_be_4[(sizeof(battle_ai_action_rank_t) == 4) ? 1 : -1];

/* Cardinal map-tile deltas stored as wrapping unsigned bytes. */
typedef union battle_ai_facing_tile_offsets {
    u8 bytes[8];
    u8 by_direction[4][2];
} battle_ai_facing_tile_offsets_t;

/* 0x80193d70: copied to the stack at 0x801984ec, then read as unsigned
 * status IDs. Signed byte fields retain the target's three-byte copy. */
enum {
    BATTLE_AI_RECOVERY_STATUS_COUNT = 3,
};

typedef struct battle_ai_recovery_status_list {
    s8 ids[BATTLE_AI_RECOVERY_STATUS_COUNT];
} battle_ai_recovery_status_list_t;
typedef char battle_ai_recovery_status_list_must_be_3
    [sizeof(battle_ai_recovery_status_list_t) == BATTLE_AI_RECOVERY_STATUS_COUNT ? 1 : -1];

extern s32 g_battle_ai_propagation_resume_pass;
extern u16 g_battle_ai_range_row_masks[];
extern s32 g_battle_ai_self_target_origin_stage;
extern battle_ai_unit_decision_t* g_battle_ai_acting_unit_decision_ptr; /* data.acting_unit_decision */
extern u8 g_battle_ai_acting_unit_id;                                   /* 0x801a01f2; data.acting_unit_id */
extern u8 g_battle_ai_acting_unit_remaining_clockticks;
extern battle_ai_command_t g_battle_ai_command_history[2];             /* 0x801a0d94, 0x801a0dac */
extern battle_ai_ability_flags_t g_battle_ai_considered_ability_flags; /* data.considered_ability.ai_flags */
extern u8 g_battle_ai_considered_ability_flags_1;
extern battle_ai_coords_t g_battle_ai_considered_coords; /* 0x801a0038; data.considered_unit_coords */
extern s32 g_battle_ai_cowardly_movement_step;
extern u8 g_battle_ai_current_ability_data_backup[];
extern u8 g_battle_ai_current_action_base_hit_percent;
extern u8 g_battle_ai_current_action_rank_byte; /* 0x8019f3f4; data.current_action.rank_byte */

/* One shared AI workspace at 0x8019f3c4, not a separate record per candidate.
 * The byte symbols annotated `data.*` alias fields of this object; they allocate no
 * additional storage. Preserve resume-state writes before returning -1. */
extern battle_ai_data_t g_battle_ai_data_base;
extern battle_ai_facing_tile_offsets_t g_battle_ai_facing_tile_offsets; /* 0x8019f358 */
extern u8 g_battle_ai_hit_counter;                                      /* data.hit_counter */
extern s32 g_battle_ai_math_ability_index;
extern u16* g_battle_ai_math_ability_list;
extern s32 g_battle_ai_math_skillset;
extern s32 g_battle_ai_math_skillset_index;
extern u8 g_battle_ai_movement_scenario;        /* data.movement_scenario */
extern u8 g_battle_ai_outcome_evaluation_state; /* 0x801a0034; data.outcome_evaluation_state */
extern battle_ai_recovery_status_list_t g_battle_ai_recovery_status_ids;
extern s32 g_battle_ai_recovery_status_index; /* 0x8019f380; status-list cursor preserved for resume */
extern s32 g_battle_ai_recovery_status_retry; /* 0x8019f37c; skip status pass after one failed attempt */
extern s32 g_battle_ai_retreat_phase;         /* 0x8019f390; retreat child resume stage */

/* 0x8019f384–0x8019f38c; cancellation-search accounting survives suspension. */
extern s32 g_battle_ai_status_candidate_count;
extern s32 g_battle_ai_status_insufficient_mp;

/* Signed weights for status IDs 0..39, read by 0x8019d864–0x8019d988. */
extern s16 g_battle_ai_status_priority_weights[40]; /* 0x8019f308..0x8019f357 */
extern s32 g_battle_ai_status_sufficient_mp;
extern battle_stats_t* g_battle_ai_temp_unit_data;
extern u8 g_battle_ai_unit_crystal_treasure_status; /* data.crystal_treasure_status */
extern u8 g_battle_ai_unit_enemy_flag;

/* Shares the 0x80193924 scratchpad-save arena used during movement propagation. */
extern battle_ai_extended_snapshot_t g_battle_ai_unit_snapshot_storage[];
extern battle_ai_extended_snapshot_t* g_battle_ai_unit_snapshots; /* 0x8019f3c0 */
extern battle_ai_data_t* g_battle_ai_workspace;                   /* 0x8019f3ac */

/* BATTLE-resident state that the EVENT, EFFECT and WORLD overlays also read. */
extern void* g_battle_ai_workspace_ptr; /* pointer cell reloaded per subsystem */
s32 battle_ai_decide_status_ct_based(s32 limit, s32 unit_id);
s32 battle_ai_set_movement_panel_data(s32 movement_taken);
void battle_ai_set_ability_considerations(s32 action_taken);
void battle_ai_evaluate_linear_ability_behaviors(void);
s32 battle_ai_build_command(s32 unit_id, battle_ai_command_t* command);
void battle_ai_build_monster_skill_tile_mask(void);
s32 battle_ai_build_movement_scenarios(void);
void battle_ai_build_targetable_tile_mask(battle_ai_tile_mask_mode_e mode, s32 radius);
s32 battle_ai_build_targeted_movement(void);
void battle_ai_build_unit_ability_list(s32 unit_id);
s32 battle_ai_calculate_ability_range(void);
s32 battle_ai_calculate_clockticks_until_death_counter_expires();
s32 battle_ai_calculate_clockticks_until_unit_acts(battle_stats_t* unit);
s32 battle_ai_calculate_distance_between_units(battle_stats_t* a, battle_stats_t* b);
s32 battle_ai_calculate_ratio_times_4(s32 divisor, s32 dividend);
s32 battle_ai_calculate_height_difference_between_units(battle_ai_coords_t* coords, battle_stats_t* unit);
s32 battle_ai_call_ability_processing(battle_ai_command_action_t* action);
s32 battle_ai_can_elemental_ability_be_used(battle_ai_coords_t* coords);
s32 battle_ai_should_exclude_from_imminent_enemies(s32 unit_id);
s32 battle_ai_can_unit_be_targeted_cryst_trea_mount_trans(s32 unit_id);
s32 battle_ai_check_ability_use_at_coords(battle_ai_coords_t* coordinates);
s32 battle_ai_check_ability_use_based_on_ct(s32 unit_index);
s32 battle_ai_check_facing_tile(battle_ai_coords_t* source, battle_ai_coords_t* target);
s32 battle_ai_can_cancel_current_status(battle_stats_t* unit, s32 status_bit);
s32 battle_ai_is_status_active_through_delay(s32 delay, battle_stats_t* unit, s32 status_id);
s32 battle_ai_check_map_allows_use_and_find_highest_target(void);
s32 battle_ai_check_set_highest_unit_priority(void);
s32 battle_ai_check_target_type(s32 unit_id);
void battle_ai_check_unit_for_crystal_or_treasure_status(void);
s32 battle_ai_check_unit_gets_turn_without_status(battle_stats_t* unit, s32 status);
s32 battle_ai_choose_move_from_move_list(void);
s32 battle_ai_choose_random_action(void);
s32 battle_ai_choose_status_action(void);
s32 battle_ai_choose_wait_facing(void);
battle_ai_ability_effect_e battle_ai_classify_ability_effect(s32 unit_id, s32 check_reflect);
void battle_ai_clear_current_action_rank_byte(void);

battle_ai_rank_order_e battle_ai_compare_target_priority_and_hit_rate(
    battle_ai_action_rank_t* a, battle_ai_action_rank_t* b);

s32 battle_ai_count_flagged_tiles_within_map_bounds(u16* rows);
void battle_ai_disable_ability_if_only_inflicts_status(battle_ai_ability_entry_t* entry, s32 status_id);
s32 battle_ai_evaluate_ability_outcome(void);
s32 battle_ai_evaluate_charging_movement(void);
s32 battle_ai_evaluate_math_targets(void);
s32 battle_ai_evaluate_movement_origins(void);
s32 battle_ai_evaluate_reflected_target_origins(void);
s32 battle_ai_evaluate_self_target_origins(void);
s32 battle_ai_evaluate_status_cancellation(s32 unit_id, s32 status_id);
s32 battle_ai_evaluate_target_tiles(void);
void battle_ai_fill_foe_proximity_grid(void);
s32 battle_ai_fill_target_distance_grid(battle_ai_coords_t* target);
s32 battle_ai_filter_reachable_tiles_by_target_distance(s32 distance_limit);
battle_ai_direction_e battle_ai_find_direction_of_target(const s8* target, const s8* origin);
s32 battle_ai_find_nearest_target(battle_ai_nearest_target_mode_e mode);
s32 battle_ai_find_unit_at_coordinates(battle_ai_coords_t* coords);
s32 battle_ai_get_movement_range(void);
s32 battle_ai_handle_autobattle(void);
s32 battle_ai_has_any_unit_decided_to_use_ability(void);
s32 battle_ai_has_any_unit_on_target_panel(void);
void battle_ai_init_acting_unit_data(void);
void battle_ai_init_selected_action(void);
void battle_ai_init_target_consideration(void);
void battle_ai_init_unit_abilities(s32 unit_id);
void battle_ai_init_workspace(void);
void battle_ai_insert_ranked_action(void);
void battle_ai_invert_target_priority(void);
s32 battle_ai_is_action_higher_ranked(battle_ai_action_data_t* a, battle_ai_action_data_t* b);
s32 battle_ai_is_vsync_hblank_past_threshold(void);
void battle_ai_load_ability_entry(battle_ai_ability_entry_t* entry);
s32 battle_ai_load_known_ability_flag(s32 unit_id, s32 skillset_id, s32 bit);

s32 battle_ai_propagate_target_movement(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget);

void battle_ai_record_considered_coords(s32 x, s32 y, s32 level);
s32 battle_ai_refresh_unit_decision_flags(void);
void battle_ai_reset_action_ranking_fields(battle_ai_action_data_t* action);
void battle_ai_restore_considered_action_data(void);
void battle_ai_restore_unit_state(s32 mode);
s32 battle_ai_run_cowardly_movement(void);
s32 battle_ai_run_non_specific_autobattle(void);
void battle_ai_move_temp_unit_to_coords(battle_ai_coords_t* coordinates);
s32 battle_ai_save_fading_life_auto_battle(void);
void battle_ai_save_unit_state(s32 mode);
s32 battle_ai_score_reachable_tiles(void);
s32 battle_ai_select_active_units(battle_ai_unit_filter_e mode, s32 target_id);
void battle_ai_select_candidate_coords_and_check_crystal(void);
s32 battle_ai_select_destination(s32 max_distance, battle_ai_destination_order_e order);
void battle_ai_select_destination_with_nearest_fallback(s32 range);
void battle_ai_select_destination_with_range_floor(s32 range);
s32 battle_ai_select_initial_action(void);
s32 battle_ai_select_peril_target(void);
s32 battle_ai_select_ranked_action(void);
s32 battle_ai_select_recovery_action(void);
s32 battle_ai_select_retreat_action(void);
s32 battle_ai_select_targeted_action(void);
s32 battle_ai_simulate_action_and_score(void);
void battle_ai_set_candidate_and_actor_reachable_tiles(void);
s32 battle_ai_exclude_enemies_acting_before_ct(void);
void battle_ai_simulate_movement_and_pickup(void);
void battle_ai_simulate_strikes(battle_ai_command_action_t* action, s32 reaction_mode);
void battle_ai_store_considered_action_data(void);
void battle_ai_store_main_target_id_and_focus_on_target_flag(s32 unit_id);
void battle_ai_store_weapon_attack_data(battle_ai_weapon_data_t* out, s32 item_id, s32* flags_out);
void battle_ai_take_next_movement_origin(void);
s32 battle_ai_take_random_tile(u16* tiles, battle_ai_coords_t* coordinates, s32 count);
void battle_ai_transfer_ability_data_and_set_defend_flag(void);
void battle_ai_transfer_byte_values(u8* destination, const u8* source, s32 byte_count);
void battle_ai_transfer_halfword_values(u16* destination, u16* source, s32 byte_count);
void battle_ai_transfer_unit_coordinates(s32 unit_index, battle_ai_coords_t* destination);
void battle_ai_update_team_targeting_flags(void);
s32 battle_ai_add_unique_value_to_list(s32 mode, s16 value, s32 limit);
s32 battle_ai_add_usable_skillset_abilities(s32 unit_id, s32 skillset, s32 count);
void battle_ai_clear_words(s32* destination, s32 byte_count);

/* animation */
typedef enum battle_facing {
    BATTLE_FACING_SOUTH = 0x000,
    BATTLE_FACING_WEST = 0x400,
    BATTLE_FACING_NORTH = 0x800,
    BATTLE_FACING_EAST = 0xc00,
    BATTLE_FACING_QUARTER_TURN = 0x400,
    BATTLE_FACING_MASK = 0x0fff,
} battle_unit_facing_e;

/* Word-view masks for the high motion byte at misc-unit +0x83. */
typedef enum battle_motion_flags {
    BATTLE_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS = 0x02000000,
    BATTLE_MOTION_FLAG_SUPPRESS_PALETTE_UPDATE = 0x04000000,
    BATTLE_MOTION_FLAG_ALTERNATE_SHORT_HOP = 0x08000000,
    BATTLE_MOTION_FLAG_FLOAT = 0x10000000,
} battle_motion_flags_e;

/* Provisional effect state view used by the two keyframe-action dispatchers. */
typedef struct battle_keyframe_effect_state {
    u8 _unused_00[2];
    s16 callback_arg_02; /* Passed as callback arg0 by the 0x801a4000 dispatcher. */
    u8 _unused_04[0x22 - 4];
    u8 callback_state[7]; /* 0x22 */
    u8 _unused_29[0xd4 - 0x29];
    s32 callback_ptrs[7]; /* 0xd4 */
} battle_keyframe_effect_state_t;

extern u8 g_battle_rotation_speed_frames[];
extern u16 g_battle_animation_speed_forced; /* 1 while the event speed is forced */
extern s32 g_animation_speed;
void animation_exception_handler(s32 exception_id);
void battle_rotate_unit(const u8* parameters);

/* dead unit */
/* 0x00 selects which panel variant runs; 0x52 is the roster/item id. */
typedef struct dead_unit_context {
    s32 kind; /* 0x00 */
    u8 _unused_04[0x4e];
    u8 item_id;     /* 0x52 */
    u8 flags[0x40]; /* 0x53 */
} dead_unit_context_t;

extern s32 g_dead_unit_action;
extern dead_unit_context_t* g_dead_unit_context;
extern s32 g_dead_unit_result;

/* deploy */
/* Formation row read by battle_unit_init_deployed_units_data: five 4-byte
 * map coordinate rows at 0x8017f388. */
typedef struct battle_deployed_coords {
    u8 party_id;               /* +0: unit to deploy */
    u8 x;                      /* +1 */
    u8 y;                      /* +2 */
    u8 facing_elevation_flags; /* +3; bit 0x10 marks unit absent before graphics/status setup */
} battle_deployed_coords_t;

extern const RECT g_deployment_clear_rect;
extern u16 g_battle_deployed_unit_palettes[2][16];
extern s32 g_battle_deployment_skipped;

/* input */
extern s32 g_battle_cursor_x;
extern s32 g_battle_cursor_y;
extern s32 g_battle_cursor_z;
extern s32 g_controller_input_copy_12;
extern u16 g_battle_cursor_screen_x;
extern s32 g_battle_cursor_previous_x;     /* cursor X before this step */
extern s32 g_battle_cursor_previous_y;     /* cursor Y before this step */
extern s32 g_battle_cursor_previous_z;     /* cursor level before this step */
extern u32 g_battle_cursor_repeat_counter; /* free-cursor repeat counter */
extern s16 g_battle_cursor_screen_y;
extern s32 g_battle_controller_input;
extern s32 g_controller_input_pressed;
extern s32 g_controller_input_previous;
extern s32 g_controller_input_raw;
extern u32 g_controller_input_released;
extern u32 g_controller_pressed_copy_0;
extern u32 g_controller_pressed_copy_1;
extern u32 g_controller_pressed_copy_2;
extern u32 g_controller_pressed_copy_3;
extern u32 g_controller_previous_copy_0;
extern u32 g_controller_previous_copy_1;
extern u32 g_controller_previous_copy_2;
extern u32 g_controller_previous_copy_3;
extern u32 g_controller_stable_frames;

/* item */
extern u8 g_battle_weapon_attack_animations[][3];
extern s16 g_battle_item_icon_clut_base_x;
extern s16 g_battle_item_icon_clut_base_y;
extern world_image_location_t g_battle_item_icon_image_location;
void battle_get_item_graphic_data(SPRT* sprite, s32 item_id);
u8* get_item_data_pointer(void);

/* script */
/* Per-byte status removal masks applied while an event stages a unit: three
 * eight-byte sets, of which the first BATTLE_STATUS_BYTE_COUNT bytes are used
 * (0x80169758). */
enum {
    BATTLE_EVENT_STATUS_MASK_BATTLE_STARTED = 0,
    BATTLE_EVENT_STATUS_MASK_PRE_BATTLE = 1,
    BATTLE_EVENT_STATUS_MASK_PRE_BATTLE_UNDEAD = 2,
    BATTLE_EVENT_STATUS_MASK_COUNT = 3,
};

extern s16 g_battle_script_attack_entry_mode;
extern s32 g_battle_script_condition_unit_battle_id;
extern s32 g_battle_script_rand16_state;
extern u32 g_battle_script_unfiltered_controller_input;
extern const char g_battle_script_pause_message[];
extern s16 g_battle_script_debug_paused;
extern s16 g_battle_event_effect_target_x;
extern s16 g_battle_event_effect_target_y;
extern u8 g_battle_script_condition_param_lengths[];
extern u16 g_battle_script_map_darkness_durations[];
extern u16 g_battle_script_unit_filter_modes[];
extern s32 g_battle_script_event_input;
extern s32 g_battle_script_saved_event_input;
extern s16 g_battle_event_map_jump_in_request;
extern s16 g_battle_script_threads_idle;
extern s32 g_battle_next_event_id;
extern u16 g_battle_script_frame_result_override;
extern VECTOR g_battle_script_light_scale;
extern s32 g_battle_event_pending_loaded_evtchr_slot;
extern u8 g_battle_script_light_base_direction_matrix[];
extern void* g_battle_event_loaded_evtchr_buffer;
extern SVECTOR g_battle_script_light_angles;
extern u8 g_battle_script_light_base_color_matrix[];
extern u8 g_battle_event_status_masks[BATTLE_EVENT_STATUS_MASK_COUNT][8];

extern u8
    g_battle_event_thread_status_snapshot[]; /* per-thread running flags sampled before the yield, indexed 1..15 */

/* Biased base: encoded event-unit IDs 0x64..0x67 reach the four halfwords at
 * g_battle_event_unit_slots without subtracting 0x64 in the target. */
extern u16 g_battle_event_unit_slots_by_encoded_id[];
extern s16 g_battle_scenario_event_active;
extern u32 g_battle_script_event_current_input;
extern u32 g_battle_script_event_initial_repeat_counter;
extern u32 g_battle_script_event_previous_input;
extern u32 g_battle_script_event_repeat_counter;
extern u32 g_battle_script_event_secondary_repeat_counter;
extern s32* g_battle_script_variables;
extern u16 g_battle_scenario_music_1_id;
extern u16 g_battle_scenario_music_2_id;
extern u16 g_event_input_suppression_frames;
extern s32 g_event_mode;

/* BATTLE pointer slot 0x80173ca4; WORLD pointer slot 0x801cd75c. */
extern event_file_block_t* g_battle_event_block;
extern u16 g_battle_event_effect_target_misc_id; /* 0x80165ff8 */

/* 0x80166000: consumer passes mode 2 when equal to 2, otherwise mode 0.
 * The last Effect operand is not a second Y coordinate. */
extern s16 g_battle_event_effect_target_mode;
extern u16 g_battle_event_map_command_80_arg1;              /* 0x80173c94 */
extern u16 g_battle_event_map_command_80_arg2;              /* 0x80173c96 */
extern u16 g_battle_event_map_command_83_arg1;              /* 0x80174058 */
extern u16 g_battle_event_map_command_83_arg2;              /* 0x8017405a */
extern u16 g_battle_event_music_switch_fade;                /* 0x80173f56; consumer multiplies by 4 */
extern u16 g_battle_event_music_switch_volume;              /* 0x80173f54; scaled 0..96 to 0..127 */
extern s16 g_battle_event_pending_effect_id;                /* 0x80173cb4 */
extern s32 g_battle_event_pending_evtchr_save_slot;         /* 0x80173f4c */
extern s32 g_battle_event_pending_loaded_evtchr_clear_slot; /* 0x80173ca0 */
extern s16 g_battle_event_pending_map_jump_out_2_id;        /* 0x8017405c */

/* Deferred BATTLE commands: producer 0x80143bd8, consumer
 * 0x80143418..0x801439c0. The outer update at 0x80142d58 clears one-shot
 * requests, schedules script threads, then applies commands on its normal
 * path. These are separate globals, not a contiguous struct. Signed pending
 * slots use -1; request bits and polled states do not. */
extern s16 g_battle_event_pending_map_jump_out_id;            /* 0x80174054 */
extern s16 g_battle_event_pending_map_state;                  /* 0x80173f64; script variable 0x24 */
extern s32 g_battle_event_pending_reserved_vram_release_slot; /* 0x80173f48 */
extern s32 g_battle_event_pending_saved_evtchr_clear_slot;    /* 0x80173f50 */

/* 0x80173cac: packed misc ID << 8 | reserved VRAM slot; -1 absent. */
extern s32 g_battle_event_pending_unit_vram_copy;
extern s32 g_battle_event_speed;
extern s16 g_battle_event_unit_slots[4];

/* 0x80173f68: packed weather flags; consumer replaces it with a table index. */
extern s16 g_battle_event_weather_request;

/* Operand-byte counts, excluding the opcode; not per-operand width schemas.
 * BATTLE 0x8014d170, WORLD 0x8013a454. */
extern u8 g_battle_script_event_instruction_sizes[EVENT_OPCODE_COUNT];

/* 0x801660a3: suppress recursive write filtering while getters use operand 0. */
extern u8 g_battle_script_variable_write_guard;
extern s32 g_battle_event_frame_counter;

/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
extern map_background_gradient_colors_t g_battle_event_background_colors;

/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
void battle_script_advance_date_by_one_year(void);
void battle_script_apply_input_overrides(u32* input);
void battle_script_assign_bitset_flag(u32* bitset, s32 bit_index, s32 value);
void battle_script_blue_remove_unit(s32 unit_id);
void battle_script_blueremoveunit_all_enemy_units(void);
void battle_script_change_stats(const u8* parameters);
void battle_script_clamp_s32_to_range(s32* value, s32 minimum, s32 maximum);
void battle_script_earthquake_start(void);
s32 battle_script_face_tile();
void battle_script_face_unit(const u8* parameters, s32 second_only);
void battle_script_filter_controller_input(u32* input);
void battle_script_focus(u8* parameters);
void battle_script_focus_speed(u8* parameters, s32* position, s32* rotation);
u32* battle_script_get_controller_input_pointer(s32 use_current);
s32 battle_script_get_current_scenario_finish_operation(void);
s32 battle_script_get_variable(s32 variable_id);
s32 battle_script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value);
void battle_script_join_units_silently_without_needing_darkscreen(void);
void battle_script_load_event(s32 id);

/* The byte-stream reader at 0x80146078 preserves signed halfwords. Some
 * callers forward their incoming a0 without an additional argument move.
 * unit_t lookup at 0x80133158 accepts and returns full words; narrowing is
 * caller-specific, not part of its interface. */
s16 battle_script_load_halfword(const u8* data);
s32 battle_script_load_next_event(void);
void battle_script_map_darkness(void);
s32 battle_script_march_units();
void battle_script_print_debug_message(void);
void battle_script_reset_event_state(void);
void battle_script_run_next_event(void);
void battle_script_seed_random_from_vsync(void);
void battle_script_set_variable(s32 variable_id, s32 value);
void battle_script_show_graphic(void);
void battle_script_sprite_move(void);
void battle_script_sprite_move_beta(void);
void battle_script_start_current_event_thread(void);
void battle_script_switch_tutorial_thread_for_eventend(void);
void battle_script_unit_anim(u8* parameters);
u32 battle_script_get_event_finish_operation(void);
void battle_script_store_halfword(u8* destination, s16 value);
void battle_script_update_event_input_state(u32 state);
void battle_script_wait_for_unit_ready(s32 misc_id);
void battle_script_warp_unit(const u8* parameters);
s32 battle_script_set_units_movement_effect_suppression();
s32 battle_script_run_scenario_conditions(void);
s32 battle_script_check_scenario_condition(s32 a, s32 b, s32 c, s32 d, s32 e);
void battle_script_run_sprite_move(s32 arg, s32 use_speed);

/* Returns the event result that the battle state handlers switch on;
 * proven by 0x80078fb4 and 0x800785ac, which both consume it. */
s32 battle_script_run_event_frame(u32* ot, u32 buttons);
void battle_script_load_event_input_state(const struct battle_event_input_state* saved);
void battle_script_clear_indexed_variable_bit0(void);
void battle_script_copy_32_bytes(void* destination, const void* source);
s32 battle_script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode);
u32 battle_script_get_event_initial_repeat_counter(void);
s32 battle_script_is_deployment_running(u32* ot, u32 buttons);
void battle_script_process_pending_requests(void);
s32 battle_script_scale_music_volume(s32 x);
void battle_script_pulse_tutorial_wait_value(s32 value);
void battle_script_set_specialized_map_destroyed(void);
void battle_dismiss_unit_event_instruction(s32 unit_id);

s32 battle_script_add_ghost_unit_event_instruction(
    s32 map_x, s32 map_y, s32 map_level, u16 map_height, s32 portrait_id, s32 misc_id, s32 flags);

/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
void battle_script_add_unit_start_thread(void);
s32 battle_script_check_unit_moving_event_instruction(s32 misc_id);
void battle_script_color_screen_thread(void);
void battle_script_color_unit_event_instruction(u8* ptr);
void battle_script_toggle_message_portrait_flip(u8* ptr);
void battle_script_set_units_palette_update_suppression(s32 unit_id, s32 enable);
void battle_script_apply_relative_camera(u8* p, s32* src);
void battle_script_load_attack_graphics_event_instruction(void);
void battle_script_execute_display_conditions_instruction(void);

/* Whole matching interpreter ranges:
 * BATTLE 0x80143bd8..0x80145f78, WORLD 0x800f6f20..0x800f92a0 (exclusive). */
void battle_script_execute_event(void);
s32 battle_script_find_instruction_byte_offset(s32 offset, s32 opcode);

/* 0x80149d6c..0x80149ebc: returns offset after the matching target marker;
 * missing targets stop the current thread. alternate_opcode accepts -1. */
s32 battle_script_find_jump_target(s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id);
s32 battle_script_get_rand16(void);
s32 battle_script_get_variable_bit_position(s32 variable_id);

/* Encoded variable IDs select words, bits, or nibbles. The command runner's
 * fourth argument is supplied by existing callers but is not read. */
s32* battle_script_get_variable_word_pointer_from_id(s32 variable_id);
void battle_script_inflict_status_thread(void);
s32 battle_script_is_tutorial_event_slot(void);
void battle_script_load_portrait_colors_event_instruction(s32 portrait_id);
void battle_script_mirrorsprite_event_instruction(u8* parameters);
void battle_script_pause_event_instruction(void);
void battle_script_play_effect_thread(void);
void battle_script_resetpalette_event_instruction(const u8* parameters);
void battle_script_run_condition(event_opcode_e opcode);
void battle_script_run_variable_command(s32 opcode, s32 destination_id, s32 source, s32 unused);
void battle_script_set_event_speed(s32 speed);
void battle_script_switch_tutorial_thread_for_event_instructions(void);
void battle_script_teleportin_event_instruction(s32 unit_id, s32 unused);
void battle_script_teleportout_event_instruction(s32 arg, s32 remove);
void battle_script_unit_animation_rotate_event_instruction(const u8* parameters);
void battle_script_unlockdate_event_instruction(s32 bitset, s32 date_index, s32 month, s32 day);
void battle_script_wait_value_event_instruction(u8* parameters);
void battle_script_waitrotateunit_and_waitrotateall_event_instruction(s32 unit_id);
void battle_script_waitspritemove_event_instruction(s32 unit_id);
void battle_script_waitwalk_event_instruction(s32 unit_id);
void battle_script_walk_to_thread(const event_walk_to_parameters_t* parameters);
void battle_script_warp_unit_display_to_paired_unit(s32 misc_id);

/* status */
/* Bit flags in status_infliction_data_t.type and the active-ability copy. The
 * target status applicator checks these in descending-bit precedence. */
typedef enum battle_status_infliction_type {
    BATTLE_STATUS_INFLICTION_TYPE_CANCEL = 0x10,
    BATTLE_STATUS_INFLICTION_TYPE_SEPARATE = 0x20,
    BATTLE_STATUS_INFLICTION_TYPE_RANDOM_ONE = 0x40,
    BATTLE_STATUS_INFLICTION_TYPE_ALL_OR_NOTHING = 0x80,
} battle_status_infliction_type_e;

/* Event infliction helpers number bits LSB-first, unlike stored status IDs. */
#define BATTLE_STATUS_LSB_INDEX(id) (((id) & ~7) | (7 - ((id) & 7)))

#define BATTLE_STATUS_LSB_MASK(id) ((u8)(1U << ((id) & 7)))

/* The special-status callback table reserves index 0; timed slots begin at Poison. */
#define BATTLE_STATUS_HANDLER_INDEX(id) ((id) + 1)

#define BATTLE_STATUS_CT_INDEX(id) ((id) - BATTLE_STATUS_ID_POISON)

#define BATTLE_STATUS_PACKED_MASK(id) ((u32)BATTLE_STATUS_BYTE_MASK(id) << (BATTLE_STATUS_BYTE_INDEX(id) * 8))

extern const u8 g_battle_status_display_image_ids[BATTLE_STATUS_COUNT];
extern s32 g_battle_status_menu_open;
extern SVECTOR g_battle_status_bubble_zoom;

/* Texture coordinates for the status bubble graphics, indexed by status bubble
 * id (0x800949dc/0x800949f4) and by a counter digit (0x80094a0c/0x80094a24).
 * Each table holds 24 bytes; the pairs are read together as one u/v. */
extern u8 g_battle_status_bubble_u[24];
extern u8 g_battle_status_bubble_v[24];
extern u8 g_battle_status_bubble_digit_u[24];
extern u8 g_battle_status_bubble_digit_v[24];
s32 battle_status_check_unit(battle_stats_t* unit);
void battle_status_disable_acting(battle_stats_t* unit);
void battle_status_enable_special_flags(s32 status_id, s32 enabled, s32 misc_unit_id);
void battle_status_queue_current_status_graphics(s32 battle_id, s32 enabled);
s32 battle_status_inflict_defending_to_battle_id(s32 unit_id);
s32 battle_status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(const battle_stats_t* unit);
s32 battle_status_modify_inflictions(s32 removal_only);
s32 battle_status_remove_charging_ability_ct(battle_stats_t* unit, s32 do_disable);
void battle_status_remove_control(battle_stats_t* unit);
s32 battle_status_resolve_unit_changes(s32 unit_id, s32 removal_only);
void battle_status_set_inflicted_ct_and_transfer_last_used_ct(s32 unit_idx);
void battle_status_store_for_current_attack(s32 unit_id, s32 removal_only);
s32 battle_status_apply_poison_and_regen(battle_stats_t* unit);
void battle_status_apply_unit_action_removal(battle_stats_t* unit);
s32 battle_status_build_upkeep_action(s32 unit_id, battle_stats_t* unit);
s32 battle_status_check_crystal_dead_jump_petrify_treasure(battle_stats_t* unit);
void battle_status_enable_disable_acting(battle_stats_t* unit);
void battle_status_inflict_by_entd_unit_id(s32 entd_id, s32 status, s32 which, s32 arg);
s32 battle_status_init_special_flag_enabling(s32 unit_id);
s32 battle_status_queue_misc_graphics_flag_change(s32 status, s32 enabled, s32 battle_id);
void battle_status_reapply_active_flags(battle_stats_t* unit);
void battle_status_remove(void);
s32 battle_status_remove_transparent(battle_stats_t* unit);
void battle_status_remove_transparent_if_jump_used(battle_stats_t* unit);
void battle_status_update_expiring(s32 unit_id);

/* system */
/*
 * Values dispatched by battle_state_run_game_loop, verified against the
 * target's switch table.  The target has no explicit handler for value 0x32.
 */
typedef enum battle_game_state {
    BATTLE_GAME_STATE_FREE_CURSOR = 0x00,
    BATTLE_GAME_STATE_FREE_CURSOR_HELP = 0x01,
    BATTLE_GAME_STATE_HIGHLIGHT_UNITS = 0x02,
    BATTLE_GAME_STATE_OPEN_ACTION_MENUS = 0x03,
    BATTLE_GAME_STATE_IDLING_ACTION_MENUS = 0x04,
    BATTLE_GAME_STATE_MENU_TO_TARGETING = 0x05,
    BATTLE_GAME_STATE_ACTION_HELP_MENU = 0x06,
    BATTLE_GAME_STATE_DISPLAY_MOVE_AREA = 0x07,
    BATTLE_GAME_STATE_MINI_MENU = 0x08,
    BATTLE_GAME_STATE_MINI_MENU_HELP = 0x09,
    BATTLE_GAME_STATE_ACTIVE_TURN = 0x0a,
    BATTLE_GAME_STATE_STATUS_EXECUTE = 0x0b,
    BATTLE_GAME_STATE_UNIT_MOVE = 0x0c,
    BATTLE_GAME_STATE_CLOSE_MOVE_HELP = 0x0d,
    BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION = 0x0e,
    BATTLE_GAME_STATE_ILLEGAL_MOVE_MENU = 0x0f,
    BATTLE_GAME_STATE_UNIT_MOVING_SETUP = 0x10,
    BATTLE_GAME_STATE_UNIT_MOVING = 0x11,
    BATTLE_GAME_STATE_MOVE_CONFIRM_MENU = 0x12,
    BATTLE_GAME_STATE_WAIT_DIRECTION = 0x13,
    BATTLE_GAME_STATE_WAIT_MENU = 0x14,
    BATTLE_GAME_STATE_CRYSTAL_LEARN = 0x15,
    BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP = 0x16,
    BATTLE_GAME_STATE_TARGETING_RANGE = 0x17,
    BATTLE_GAME_STATE_ILLEGAL_RANGE = 0x18,
    BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING = 0x19,
    BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP = 0x1a,
    BATTLE_GAME_STATE_CONFIRM_ACTION = 0x1b,
    BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION = 0x1c,
    BATTLE_GAME_STATE_ACTION_CAST = 0x1d,
    BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE = 0x1e,
    BATTLE_GAME_STATE_TARGET_SELECT_START = 0x1f,
    BATTLE_GAME_STATE_TARGET_SELECT = 0x20,
    BATTLE_GAME_STATE_TARGET_SELECT_DENIED = 0x21,
    BATTLE_GAME_STATE_TARGET_SELECT_CONFIRM = 0x22,
    BATTLE_GAME_STATE_TARGET_DISPLAY_START = 0x23,
    BATTLE_GAME_STATE_TARGET_DISPLAY = 0x24,
    BATTLE_GAME_STATE_AFTER_COMMAND = 0x25,
    BATTLE_GAME_STATE_JP_EXP_GAIN = 0x26,
    BATTLE_GAME_STATE_CHANGE_TURN = 0x27,
    BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT = 0x28,
    BATTLE_GAME_STATE_EFFECT_DAMAGE_DISPLAY = 0x29,
    BATTLE_GAME_STATE_OPEN_SP2_FILES = 0x2a,
    BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN = 0x2b,
    BATTLE_GAME_STATE_SECONDARY_EFFECT = 0x2c,
    BATTLE_GAME_STATE_ACTION_EXECUTE = 0x2d,
    BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY = 0x2e,
    BATTLE_GAME_STATE_RESUME_ATTACK_PHASE = 0x2f,
    BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD = 0x30,
    BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH = 0x31,
    BATTLE_GAME_STATE_EFFECT = 0x33,
    BATTLE_GAME_STATE_EVENT = 0x34,
    BATTLE_GAME_STATE_MAP_JUMPING_OUT = 0x35,
    BATTLE_GAME_STATE_MAP_INITIALIZE = 0x36,
    BATTLE_GAME_STATE_MAP_JUMPING_IN = 0x37,
    BATTLE_GAME_STATE_MAP_JUMPING_OUT_2 = 0x38,
    BATTLE_GAME_STATE_MAP_INITIALIZE_2 = 0x39,
    BATTLE_GAME_STATE_MAP_JUMPING_IN_2 = 0x3a,
    BATTLE_GAME_STATE_CLOSE_BATTLE = 0x3b,
} battle_game_state_e;

/* Provisional 0x14-byte system-function record, table at 0x801692c0
 * (battle_menu_run_system_function_thread, battle_menu_dispatch_system_function). */
typedef struct battle_system_function {
    s16 value_00;               /* 0x00; copied to g_battle_menu_current_id */
    u16 text_id;                /* 0x02; menu entry text id */
    s16 menu_entry_index;       /* 0x04; g_battle_menu_thread_menu_data index */
    u16 value_06;               /* 0x06; copied to g_battle_menu_system_function_row_actions */
    u16 value_08;               /* 0x08; menu entry value_20 */
    u8 _padding_0a[2];          /* aligns thread_entry */
    void (*thread_entry)(void); /* 0x0c; started as thread 8 */
    s16 alternate_id;           /* 0x10; replaces the id when navigation messages are not On */
    u8 _padding_12[2];          /* tail padding to 4-byte alignment */
} battle_system_function_t;
typedef char battle_system_function_size_must_be_0x14[(sizeof(battle_system_function_t) == 0x14) ? 1 : -1];

/* Battle heap block header: blocks are counted in 8-byte units and linked
 * either on the circular free list (g_battle_heap_rover) or on one of the 16
 * per-owner lists in g_battle_heap_owner_lists. */
typedef struct battle_heap_node {
    struct battle_heap_node* next;
    u16 size; /* in 8-byte blocks */
    s16 owner;
} battle_heap_node_t;

typedef struct battle_heap_owner_list {
    battle_heap_node_t* head;
    s32 _unused_04;
} battle_heap_owner_list_t;

/* A random value between `min` and `max`, in either order. Spelled with rand()
 * first, unlike RAND_BETWEEN; both spellings occur in the target. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

/* Array view: as a scalar, GCC hoists the menu-index reload above its store
 * in battle_menu_run_system_function_thread. */
extern battle_system_function_t g_battle_system_function_table[];
extern s32 g_battle_state_map_init_step;
extern u8 g_battle_random_battle_music_tracks[];
extern s32 g_battle_system_function_thread_busy;
extern battle_heap_node_t* g_battle_heap_base;
extern s16 g_battle_heap_block_count;
extern battle_heap_owner_list_t g_battle_heap_owner_lists[];
extern battle_heap_node_t* g_battle_heap_rover;
extern u32 g_battle_heap_end_address;
extern s32 g_battle_heap_min_largest_free; /* Lowest largest-free_node-block size seen, in 8-byte units. */
extern s32 g_battle_game_state;
extern s32 g_battle_state_animation_continue_check;
extern s32 g_battle_state_game_flow_running;
extern s32 g_battle_state_map_transition_step;
extern s32 g_battle_frame_counter;
extern s32 g_battle_frame_measurement;
extern int g_battle_overlay_loaded;
extern s32 g_frame_pacing;
extern s32 g_frame_pacing_suppressed;
extern s32 g_frame_pacing_timer;
void battle_state_enter_action_cast(void);
void battle_state_enter_commence_attack_phase(void);
void battle_noop_8018ef2c(void);
void battle_state_enter_open_sp2_files(void);
void battle_bin_entrypoint(void);
s32 battle_mul_div_s64(s32 a, s32 b, s32 c);
void battle_noop_8012e190(void);
void battle_noop_80143bd0(void);
void battle_state_enter_pre_attack_animation(void);
void* game_realloc(void*, u32);
void battle_noop_80079298(void);
void battle_noop_8007aecc(void);
void battle_noop_8013ccec(void);
void battle_noop_8013da70(void);
void battle_noop_8014a82c(void);

/* BATTLE 0x8014bf54..0x8014bf7c: destination first. This do-while loop
 * requires a positive count; it is not a general-purpose memcpy. */
void battle_copy_bytes(void* destination, const void* source, s32 count);
void* battle_heap_alloc_block(u32 size, s32 kind);
void battle_heap_free_block(void* ptr);
void battle_heap_init(battle_heap_node_t* buffer, u32 buffer_size);
void battle_heap_print_stats(void);
void battle_state_run_deployment(void);
void battle_state_enter_action_execution_setup(void);
void battle_state_enter_after_command(void);
void battle_state_enter_highlight_units_by_team(void);
void battle_state_enter_jp_xp_gain(void);
void battle_state_enter_status_execute(void);
void battle_state_enter_target_display(void);
void battle_state_enter_target_out_of_range(void);
void battle_state_enter_target_select(void);
void battle_state_enter_target_select_confirm(void);
void battle_state_enter_target_select_denied(void);
void battle_state_enter_unit_moving(void);
void battle_state_stop_map_animations(void);
s32 battle_state_get_animation_speed(void);
void battle_state_handle_ability_preview_help_state(void);
void battle_state_handle_ability_preview_state(void);
void battle_state_handle_action_cast_state(void);
void battle_state_handle_action_execute_setup_state(void);
void battle_state_update_action_execute_mode(void);
void battle_state_handle_action_help_menu_state(void);
void battle_state_handle_active_turn_state(void);
void battle_state_handle_after_command_state(void);
void battle_state_handle_battle_message_display_state(void);
void battle_state_handle_change_turn_state(void);
void battle_state_handle_close_battle_state(void);
void battle_state_handle_close_move_help_state(void);
void battle_state_handle_commence_attack_phase_state(void);
void battle_state_handle_confirm_action_state(void);
void battle_state_handle_crystal_learn_state(void);
void battle_state_handle_deep_dungeon_mesh_finish_state(void);
void battle_state_handle_deep_dungeon_mesh_load_state(void);

/* Per-frame handlers dispatched by battle_state_run_game_loop. */
void battle_state_handle_default_state(void);
void battle_state_handle_display_move_area_state(void);
void battle_state_handle_effect_damage_display_state(void);
void battle_state_handle_effect_state(void);
void battle_state_update_event_mode(void);
void battle_state_handle_free_cursor_help_state(void);
void battle_state_handle_free_cursor_input(void);
void battle_state_handle_highlight_units_state(void);
void battle_state_handle_idling_action_menus_state(void);
void battle_state_handle_illegal_move_menu_state(void);
void battle_state_handle_illegal_range_state(void);
void battle_state_handle_jp_exp_gain_state(void);
void battle_state_handle_learn_ability_on_hit_state(void);
void battle_state_handle_change_map_init_state(void);
void battle_state_handle_map_init_state(void);
void battle_state_handle_change_map_jumping_in_state(void);
void battle_state_handle_map_jumping_in_state(void);
void battle_state_handle_change_map_jumping_out_state(void);
void battle_state_handle_map_jumping_out_state(void);
void battle_state_handle_menu_to_targeting_state(void);
void battle_state_handle_mini_menu_help_state(void);
void battle_state_handle_mini_menu_state(void);
void battle_state_handle_move_confirm_menu_state(void);
void battle_state_handle_move_range_exception_state(void);
void battle_state_handle_open_action_menus_state(void);
void battle_state_handle_open_sp2_files_state(void);
void battle_state_handle_pre_attack_animation_state(void);
void battle_state_handle_resume_attack_phase_state(void);
void battle_state_handle_secondary_effect_state(void);
void battle_state_handle_start_effect_file_open_state(void);
void battle_state_handle_status_execute_state(void);
void battle_state_handle_target_display_start_state(void);
void battle_state_handle_target_display_state(void);
void battle_state_handle_target_select_confirm_state(void);
void battle_state_handle_target_select_denied_state(void);
void battle_state_handle_target_select_start_state(void);
void battle_state_handle_target_select_state(void);
void battle_state_handle_targeting_range_state(void);
void battle_state_handle_unit_move_state(void);
void battle_state_handle_unit_moving_setup_state(void);
void battle_state_handle_unit_moving_state(void);
void battle_state_handle_wait_direction_state(void);
void battle_state_handle_wait_menu_state(void);
void battle_state_restart_menu_to_targeting(void);
void battle_state_run_game_loop(void);
void battle_state_start_close_battle(s32 duration);
void battle_state_start_game_flow(void);
void battle_state_stop_game_flow(void);
s32 battle_state_sync_frame(u32 ordering_table);
s32 battle_state_update_controller_input(void);
s32 battle_state_announce_next_charged_action(void);
void battle_state_enter_target_display_start(void);
void battle_state_enter_target_select_start(void);
s32 battle_state_get_animation_continue_check(void);
void battle_state_halve_animation_speed_and_queue_close(s32 transition_step, s32 close_flow_state);
void battle_state_init_deployment_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue);
s32 battle_state_set_animation_speed(s32 speed);
void battle_state_set_free_cursor(void);
void battle_state_set_time_scale(s32 value);
void battle_state_start_battle_message_display(void);
void battle_state_start_change_map_jump_in(s32 duration);
void battle_state_start_map_jump_out(s32 map_id, s32 duration);
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration);
s32 battle_state_sync_and_submit_deployment_frame(u32 ordering_table);
s32 battle_state_update_deployment_controller_input(void);
void battle_state_enter_unit_moving_setup(void);
void battle_noop_80133150(s32 unused_unit_id);
void battle_noop_80149be4(const u8* unused_parameters);

/* thread */
/* 0x8014ceb4..0x8014cf58 forwards a0-a3 to the callback in 0x80173ca8,
 * using thread 0's stack when necessary. It does not copy stacked arguments
 * across that stack switch; callers must not assume they are transferred.
 * The unspecified argument list is intentional, not a missing prototype. */
extern void (*g_battle_thread_call_target)(void);

/* Full BATTLE backing array at 0x8016986c. The pointer slot 0x80165f98 is
 * separate. */
extern native_thread_t g_battle_thread_contexts[];

/* View of g_battle_thread_contexts biased to task_id (0x801698b8): [i][0] is
 * thread i's task_id and [i][1] its task_words[0]. Target code addresses the
 * biased symbol directly, so it must not be rebound to the context base. The
 * WORLD twin is g_world_thread_task_ids. */
extern s32 g_battle_thread_task_ids[][256];

/* BATTLE's pointer slot at 0x80165f98, set to g_battle_thread_contexts by
 * battle_menu_init_subsystem_pointers. Every access indexes it with a 0x400
 * stride, and the scheduler primitives reach is_running (0x48) and task_id
 * (0x4c) through it, so it is a native_thread_t*, not the u8* placeholder it
 * was spelled as in 22 files. Files doing raw byte arithmetic cast (u8*) at
 * the use site. */
extern native_thread_t* g_battle_threads;
void battle_thread_wait_for_10_to_13(void);
void battle_block_start_thread(void);
s32 battle_thread_call_on_main_stack();
void battle_thread_exit_current(void);
s32 battle_thread_find_running_by_task(s32 task_id);
void* battle_thread_get_current_global_pointer(void);
s32 battle_thread_get_current_parameter_1(void);
s32 battle_thread_get_current_parameter_2(void);
s32 battle_thread_get_current_parameter_3(void);
s32 battle_thread_get_current_task_id(void);
s32 battle_thread_is_previous_running(void);
s32 battle_thread_is_running(s32 thread_id);
s32 battle_thread_is_running_8014cc94(s32 thread_id);
void battle_thread_reset_scheduler(void);
s32 battle_thread_resolve_id(s32 requested_thread_id);
s32 battle_thread_resolve_id_after_current(s32 requested_thread_id);
void battle_thread_resume(s32 thread_id);
void battle_thread_set_current_task_id(s32 task_id);
void battle_thread_set_parameters(s32 thread_id, s32 first, s32 second, s32 third);
void battle_thread_set_parameters_4(s32 thread_id, s32 first, s32 second, s32 third, s32 fourth);
void battle_thread_start(s32 thread_id, void (*function)(void));
void battle_thread_suspend(s32 thread_id);
void battle_thread_wait_frames(s32 count);
void battle_thread_wait_until_inactive(s32 thread_id);

/* 0x8014ca80..0x8014cbb4: save this native context and resume the next
 * active one. Shared state can change before this caller resumes. */
void battle_thread_yield(void);

/* screen */
/* Renderer-space coordinates use signed X, Z, Y halfwords. */
typedef struct battle_screen_coords {
    s16 x;
    s16 z;
    s16 y;
} battle_screen_coords_t;

typedef struct battle_action_reward_display {
    u8 earned_experience;     /* +0x0 */
    u8 earned_jp;             /* +0x1 */
    u8 level_for_display;     /* +0x2; nonzero selects the "Level UP!" display */
    u8 job_level_for_display; /* +0x3 */
} battle_action_reward_display_t;
typedef char battle_action_reward_display_size_must_be_4[(sizeof(battle_action_reward_display_t) == 4) ? 1 : -1];

/*
 * Misc unit_t Data has 16 renderer-side slots, each 0x440 bytes. This
 * declaration spans the full slot; unverified regions remain padding.
 * The record links to its simulation-side battle_stats_t at 0x134.
 */
/* Halfword tile destination written by battle_unit_find_relocation_tile into battle_unit_misc_data_t.dismount. */
typedef struct battle_dismount_coords {
    s16 x;     /* 0x0 */
    s16 level; /* 0x2 */
    s16 y;     /* 0x4 */
} battle_dismount_coords_t;

typedef union battle_action_display_flags {
    u32 word;
    u8 bytes[4];
} battle_action_display_flags_t;
typedef char battle_action_display_flags_size_must_be_4[(sizeof(battle_action_display_flags_t) == 4) ? 1 : -1];

enum {
    BATTLE_ACTION_STATUS_DISPLAY_REMOVAL = 0x80,
    BATTLE_ACTION_STATUS_DISPLAY_IMAGE_MASK = 0x7f,
    BATTLE_ACTION_STATUS_DISPLAY_CAPACITY = 0x1b,
};

typedef union battle_move_destination {
    u32 word;
    struct {
        u8 destination_x;
        u8 destination_y;
        u8 destination_z;
        u8 motion_flags;
    } bytes;
} battle_move_destination_t;

/* Source frame consumed at 0x80083e10: a 2-byte header followed by 4-byte
 * parts. Signed byte shifts precede the packed tile, dimension and flip bits.
 * The renderer's display record supplies the part count. */
typedef struct battle_gfx_source_part {
    s8 x_shift;
    s8 y_shift;
    u16 attributes;
} battle_gfx_source_part_t;

typedef struct battle_gfx_source_frame {
    /* Bits 0-2 are the part count minus one, bits 3-7 index the Y rotation
     * table g_battle_gfx_sprite_y_rotations (0x80094508). */
    u8 part_count_and_rotation;
    u8 flags;
    battle_gfx_source_part_t parts[1];
} battle_gfx_source_frame_t;

/* WEP/EFF SHP data: the first frame of each weapon type, then the frame
 * pointers from 0x40. */
typedef struct battle_gfx_weapon_shp {
    u16 first_frame[0x20];                /* 0x00; indexed by weapon type */
    battle_gfx_source_frame_t* frames[1]; /* 0x40 */
} battle_gfx_weapon_shp_t;

typedef struct battle_gfx_sprite_part_display_data {
    s8 x_shift;
    s8 y_shift;
    u8 width;
    u8 height;
    u8 u;
    u8 v;
    u8 flags;
} battle_gfx_sprite_part_display_data_t;
typedef char
    battle_sprite_part_display_data_size_must_be_7[(sizeof(battle_gfx_sprite_part_display_data_t) == 7) ? 1 : -1];

/*
 * Header of a 0x440-byte sprite display slot (unit, weapon, item, numeric and
 * status-bubble displays all share it).  The battle_gfx_sprite_part_display_data_t records
 * start at 0x0e; battle_initialize_*_sprite_display fill in the header.
 */
typedef struct battle_gfx_sprite_display_data {
    u8 red;             /* 0x00 */
    u8 green;           /* 0x01 */
    u8 blue;            /* 0x02 */
    u8 part_count;      /* 0x03; graphics count passed to battle_gfx_construct_polygon_data_for_units */
    s16 spritesheet_id; /* 0x04; 0x1e = ITEM.BIN, 0x1f = FRAME.BIN */
    u16 clut;           /* 0x06 */
    s16 scale_x;        /* 0x08; 1.0 = 0x1000 */
    s16 scale_y;        /* 0x0a; 1.0 = 0x1000 */
    s16 y_rotation;     /* 0x0c; SEQ 0xe5 saves it, 0xdf clears it; the frame header selects it */
    battle_gfx_sprite_part_display_data_t parts[1]; /* 0x0e; count depends on the display */
} battle_gfx_sprite_display_data_t;
/*
 * This is the HEADER only, not a whole slot: 0x0e + one 7-byte part record,
 * padded to the s16 alignment.  The 0x440 per-unit stride the callers use is
 * NOT a multiple of it (0x440 = 49 * 0x16 + 10), so the display symbols cannot
 * be declared as arrays of this type -- &sym[i * 0x440] would rescale.  The
 * g_battle_gfx_*_sprite_display_data symbols are strided views into one
 * 0x440-byte per-unit block, spaced 0x16 apart at unit 0 (0x800b76d6,
 * 0x800b76ec, 0x800b7702, 0x800b7718, 0x800b772e), which is what pins this
 * size independently of the C layout.
 */
typedef char
    battle_gfx_sprite_display_data_size_must_be_0x16[(sizeof(battle_gfx_sprite_display_data_t) == 0x16) ? 1 : -1];

/* Weapon/effect sprite animation block, misc unit data 0x208, 0x238 and
 * 0x268. It shares the first 0x24 bytes of battle_unit_anim_state_t but keeps
 * its display pointer at 0x24 (battle_gfx_run_wep_eff_seq_script, battle_gfx_draw_unit_sprite_layers,
 * battle_unit_init_misc_data). Its SEQ table has no header: seq[animation] is the script. */
typedef struct battle_unit_sprite_block {
    u16 trigger;                               /* 0x00 */
    u16 graphic_type;                          /* 0x02; 1 weapon (WEP1), 2 effect (EFF) */
    u16 animation_id;                          /* 0x04 */
    u16 script_pos;                            /* 0x06 */
    u16 frame;                                 /* 0x08 */
    u16 wait;                                  /* 0x0a */
    u16 counter_0c;                            /* 0x0c */
    u16 saved_animation_id;                    /* 0x0e */
    u16 saved_script_pos;                      /* 0x10 */
    u16 wait_bias;                             /* 0x12 */
    u16 frame_bias;                            /* 0x14 */
    u16 loop_count;                            /* 0x16 */
    u16 flags;                                 /* 0x18 */
    u8 _padding_1a[2];                         /* 0x1a; aligns shp */
    battle_gfx_weapon_shp_t* shp;              /* 0x1c */
    u8** seq;                                  /* 0x20 */
    battle_gfx_sprite_display_data_t* display; /* 0x24 */
    u8 _unused_28[8];                          /* 0x28 */
} battle_unit_sprite_block_t;
typedef char battle_unit_sprite_block_size_must_be_0x30[(sizeof(battle_unit_sprite_block_t) == 0x30) ? 1 : -1];

/* Manual targeting and WAIT_DIRECTION reuse the AI command storage.
 * The facing word overlaps the action's item/type/target-ID bytes. Cursor
 * coordinate readers use signed halfwords over the action's target fields. */
typedef union battle_unit_command_state {
    battle_ai_command_t ai;
    struct {
        u8 _unused_00[12];
        s32 facing_hint;
        SVECTOR target_panel; /* vx = panel x, vy = map level, vz = panel y */
    } cursor;
} battle_unit_command_state_t;
typedef char battle_misc_command_state_size_must_be_24[sizeof(battle_unit_command_state_t) == 24 ? 1 : -1];

/* Misc unit_t Data byte read as a byte or as a halfword over the next byte. */
typedef union battle_gfx_sprite_display_flags {
    u8 byte;
    u16 half;
} battle_gfx_sprite_display_flags_t;

/* Walk speed halfword pair, read and stored as one word. */
typedef union battle_move_walk_speed {
    s32 word;
    struct {
        u16 speed;
        u16 copy;
    } halves;
} battle_move_walk_speed_t;

/* Misc unit_t Data halfword read both signed and unsigned. */
typedef union battle_unit_misc_halfword {
    s16 s;
    u16 u;
} battle_unit_misc_halfword_t;

/* Unit SHP frame tables filled by battle_gfx_unpack_unit_shp_data (0x800873bc):
 * SHP header bytes 4..5 and 6..7, then two 0xd0-entry frame pointer tables.
 * The second table repeats the first when the resource carries a single
 * frame set (header 8). The frame starts are the first frame loaded from the
 * second half of the sheet and the first frame loaded from the SP2 file. */
typedef struct battle_gfx_unit_shp_frame_tables {
    s32 attack_frame_start;
    s32 sp2_frame_start;
    u8* primary[0xd0];   /* 0x008 */
    u8* secondary[0xd0]; /* 0x348 */
} battle_gfx_unit_shp_frame_tables_t;

typedef struct battle_unit_misc_data {
    struct battle_unit_misc_data* previous; /* 0x000; previous Misc unit_t Data */
    u8 unit_id;                             /* 0x004; Misc unit_t Data ID */
    u8 spritesheet_vram_slot;               /* 0x005 */
    u8 spritesheet_id;                      /* 0x006 */
    u8 stored_palette;                      /* 0x007 */
    u16 state_frame_counter;                /* 0x008; incremented while renderer action states wait */
    u16 ability_in_use;                     /* 0x00a; always accessed as a halfword */
    u16 requested_animation;                /* 0x00c */
    u16 vram_spritesheet_id;                /* 0x00e */
    u16 vram_palette_id;                    /* 0x010 */
    /* Read-modify-written as a halfword over 0x012..0x013
     * (battle_gfx_update_sprite_transparency_flag). */
    battle_gfx_sprite_display_flags_t sprite_display_flags; /* 0x012 */
    /* 0x014; SEQ opcode 0xe2, indexes g_battle_gfx_animation_layer_priorities. */
    u16 layer_priority;
    u8 _padding_016[2]; /* aligns real */
    VECTOR real;        /* 0x018; vx = x, vy = height, vz = map depth */
    /* Per-frame real-coordinate velocity; the jump-start distortion animation
     * (0x8008a35c) copies all four words as one block. */
    VECTOR velocity; /* 0x028 */
    /* walk_speed and its copy are read and stored as one word
     * (battle_unit_set_idle_animation_for_movement, battle_move_init_knockback). */
    battle_move_walk_speed_t walk_speed; /* 0x038 */
    s32 step_speed; /* 0x03c; interpolation speed of the current step; 0x2000 when a move/climb starts, raised by
                       jump gravity */
    SVECTOR screen; /* 0x040; vx = x, vy = height, vz = map depth */
    u8 _unused_048[8];
    /* Two more SVECTORs (vx = x, vy = height, vz = map depth) at 0x050 and
     * 0x060, used to create vectors for effects processing.
     * battle_unit_shift_forward_or_backward and
     * battle_unit_add_signed_byte_to_height read-modify-write the first;
     * increment_another_coords_by_misc_id(_copy) accumulate into the
     * second. */
    SVECTOR effect_vector; /* 0x050 */
    /* Screen offset added to the projected sprite position (0x80086b44) by
     * SEQ opcode 0xf9; cleared with both effect vectors by
     * battle_unit_place_in_new_location. vx/vy are the screen X/Y offsets; vz is
     * only ever cleared with them. */
    SVECTOR screen_offset;   /* 0x058 */
    SVECTOR effect_vector_2; /* 0x060 */
    u8 _unused_068[4];
    /* Camera-relative facing, ((camera yaw + facing) & 0xfff) / 1024 and / 256,
     * stored at 0x80085c0c; copied from mount to rider at 0x80069174/0x80069180;
     * read as signed halfwords by battle_get_alternate_facing_quadrant_* (% 4) and
     * battle_unit_get_facing_field_0x6e_nibble_by_misc_id (% 0x10). */
    battle_unit_misc_halfword_t camera_facing_quadrant;  /* 0x06c */
    battle_unit_misc_halfword_t camera_facing_sixteenth; /* 0x06e */
    u16 facing;                                          /* 0x070 */
    u16 attack_facing;                                   /* 0x072 */
    u16 depth_height_offset;                             /* 0x074 */
    u16 mounted_height_offset;                           /* 0x076 */
    u16 float_bob_phase;                                 /* 0x078; advanced by g_animation_speed at 0x8007ea98 */
    s16 special_graphic_y_offset;                        /* 0x07a */
    u8 map_x;                                            /* 0x07c */
    u8 map_y;                                            /* 0x07d */
    u8 map_z;                                            /* 0x07e */
    u8 centre_tile_offset;                               /* 0x07f */
    battle_move_destination_t movement;
    u8 previous_map_x;          /* 0x084 */
    u8 previous_map_y;          /* 0x085 */
    u8 previous_map_z;          /* 0x086 */
    u8 distortion_animation_id; /* 0x087; dispatcher 0x8008b234 */
    s32 distortion_phase;       /* 0x088; distortion animation state (0x8008a35c, 0x80089640) */
    s32 distortion_timer;       /* 0x08c; frames left in the jump-start rise (0x8008a35c) */
    s32 distortion_target;      /* 0x090; target real.vy or frame threshold of the distortion step */
    u16 previous_facing;        /* 0x094 */
    u8 destination_edge_height; /* 0x096; destination slope corner, as g_battle_move_destination_edge_height */
    u8 current_edge_height;     /* 0x097; current slope corner, as g_battle_move_current_edge_height */
    u32 movement_path_offset;   /* 0x098; relative to this record */
    /*
     * Movement path (0x9c/0x9d). A count of 0xfe means 0x9d..0x9f hold an explicit
     * x/y/z instead of a step list (initialize_units_coordinates_animation_facing); a
     * non-zero count means the unit is still moving (unit_moving_check_by_misc_id,
     * process_unit_movement); set_unit_movement_flag ORs 0x10 into a step.
     */
    u8 movement_path_count; /* 0x09c */
    u8 movement_path[0x7c]; /* 0x09d..0x118; step: direction | 0x20 higher elevation | jump length */
    u8 movement_flags;      /* 0x119; battle_move_effective_flags_e bits */
    u8 _unused_11a;
    u8 mount_byte;                /* 0x11b */
    u8 movement_value;            /* 0x11c */
    u8 last_path_count;           /* 0x11d; receives prior movement-path count at 0x8006d7b8 */
    u8 current_unit_id_plus_one;  /* 0x11e */
    u8 previous_unit_id_plus_one; /* 0x11f */
    s16 item_get_camera_x;        /* 0x120 */
    s16 item_get_camera_y;        /* 0x122 */
    u8 _unused_124[4];
    u32 otag_depth_index;              /* 0x128 */
    s32 status_bubble_gte_flag;        /* 0x12c; RotTrans flag output while positioning the status bubble */
    u8 mount_state;                    /* 0x130; battle_misc_mount_state_e */
    u8 mount_partner_misc_id;          /* 0x131; linked rider or mount misc ID */
    u8 previous_mount_state;           /* 0x132; battle_misc_mount_state_e */
    u8 previous_mount_partner_misc_id; /* 0x133 */
    battle_stats_t* battle_data;       /* 0x134; battle_stats_t backlink */
    u16 used_ability_id;               /* 0x138 */
    u8 used_item_or_weapon_id;         /* 0x13a */
    u8 equipped_weapon_type;           /* 0x13b */
    u8 ability_ct_resolved;            /* 0x13c */
    u8 team_flags;                     /* 0x13d */
    u8 palette_modifier;               /* 0x13e */
    u8 horizontal_flip_flag;           /* 0x13f */
    u32 status_flags_1_4;              /* 0x140; battle_misc_status_flags_1_4_e */
    u32 status_flags_5_6;              /* 0x144; battle_misc_status_flags_5_6_e */
    /* 0x148..0x157: statuses to add/remove;
     * battle_unit_update_float_and_jump_height tests the pending removal of Jump. */
    u32 statuses_to_add_1_4;                   /* 0x148 */
    u32 statuses_to_add_5_6;                   /* 0x14c */
    u32 statuses_to_remove_1_4;                /* 0x150 */
    u32 statuses_to_remove_5_6;                /* 0x154 */
    battle_unit_command_state_t command_state; /* 0x158..0x16f */
    s32 sp2_ability_id;                        /* 0x170: ability whose SP2 file battle_open_sp2 loads */
    s32 ability_preview_phase;                 /* 0x174: ability preview branch selector */
    /* 0x178; battle_target_calculate_for_menu_types result stored by
     * battle_target_select_tile: 0/1 preview, 2 stop, -1 back to the menu. */
    s32 target_select_result;
    s32 attack_phase_state;    /* 0x17c: 3 == commence, else pre-phase */
    s32 pending_attack_result; /* 0x180: attack result code; -1 signals death */
    /* Death-by-dismount destination (Miscellaneous unit_t Data 0x184/0x186/
     * 0x188); battle_unit_set_map_coords_after_death_dismount copies them to
     * map_x/map_z/map_y.  attack_result_animation_update passes &dismount. */
    battle_dismount_coords_t dismount; /* 0x184 */
    u8 _unused_18a[2];
    /* Current action data, 0x18c..0x1d7. */
    u8 action_18c;           /* 0x18c; "Reaction ID / Attacking unit ID? - Used Ability ID" */
    u8 target_count;         /* 0x18d; attacker_face_targets, update_anim_display_for_all_targets */
    u8 target_list[0x10];    /* 0x18e; misc ids, 0xff terminated */
    u8 animate_on_miss_flag; /* 0x19e; 1 when animate-on-miss is false, cleared on hit */
    u8 control_value_19f;    /* 0x19f; 0x01 math skill */
    u16 last_attack_id;      /* 0x1a0 */
    u8 ability_formula;      /* 0x1a2; can poach? */
    u8 reaction_occurred;    /* 0x1a3 */
    u8 continue_attack;      /* 0x1a4 */
    u8 current_hit_number;   /* 0x1a5 */
    u8 reaction_id_1a6;      /* 0x1a6 */
    u8 _unused_1a7;
    u8 target_new_x;         /* 0x1a8; post-action knockback destination (transfer_target_coordinates) */
    u8 target_new_y;         /* 0x1a9 */
    u8 target_new_map_level; /* 0x1aa */
    u8 used_weapon_id;       /* 0x1ab */
    s16 reaction_ability_id; /* 0x1ac; 0x1b8 (Reflect) suppresses auto-facing at 0x80072838 */
    u8 knockback_flags;      /* 0x1ae; battle_strike_work_t.knockback_flags */
    u8 can_earn_experience;  /* 0x1af */
    battle_action_reward_display_t action_rewards; /* 0x1b0..0x1b3 */
    u8 command_ready; /* 0x1b4; 0x800753b8 sets 1 after AI setup completes; gates command dispatch */
    /* 0x1b5; strikes run after the first while continue_attack is set; zeroed
     * for each new action. */
    u8 continue_attack_count;
    /* 0x1b6; command kind stored before target select (0x0c enemy, 0x0e ally
     * required) or target display (0x0d, 0x0f). */
    u8 target_select_command;
    u8 status_display_image_count;                      /* 0x1b7; capped at 0x1b */
    battle_action_display_flags_t action_display_flags; /* 0x1b8..0x1bb */
    /* Action-display image IDs, not canonical status IDs.  Bit 0x80 marks a
     * removal; the dispatcher consumes entries from the end. */
    u8 status_display_image_list[BATTLE_ACTION_STATUS_DISPLAY_CAPACITY]; /* 0x1bc..0x1d6 */
    u8 _padding_1d7;                                                     /* aligns sprite_graphic_trigger */
    /* The unit_t sprite data block starts at 0x1d8; the trigger is written
     * as a halfword by animate_and_set_enemy_level_data_by_misc_id.
     * 0x1d8..0x207 is the unit's battle_unit_anim_state_t, spelled out here. */
    u16 sprite_graphic_trigger;    /* 0x1d8 */
    u16 sprite_graphic_type;       /* 0x1da */
    u16 encoded_animation;         /* 0x1dc: half-shifted animation id */
    u16 animation_script_pos;      /* 0x1de: frame-command byte counter (anim state script_pos) */
    u16 animation_frame;           /* 0x1e0: current frame (anim state frame) */
    u16 animation_countdown;       /* 0x1e2: frames remaining in current attack state */
    u16 secondary_animation_state; /* 0x1e4 */
    u16 saved_animation_id;        /* 0x1e6 */
    u16 saved_script_pos;          /* 0x1e8 */
    u16 animation_wait_bias;       /* 0x1ea */
    u16 animation_frame_bias;      /* 0x1ec */
    u16 animation_loop_count;      /* 0x1ee */
    u16 animation_flags;           /* 0x1f0; toggled by script opcodes 0xec/0xeb */
    u8 _padding_1f2[2];            /* 0x1f2; aligns shp_data */
    /* unit_t sprite SHP/SEQ pointers (0x1f4/0x1f8); poach_morbol_transformation
     * rewrites both for the Malboro spritesheet. */
    battle_gfx_unit_shp_frame_tables_t* shp_data;             /* 0x1f4 */
    u8* seq_data;                                             /* 0x1f8 */
    battle_gfx_unit_shp_frame_tables_t* saved_shp_data;       /* 0x1fc; pre-transformation SHP pointer (0x8008363c) */
    u8* saved_seq_data;                                       /* 0x200; pre-transformation SEQ pointer */
    battle_gfx_sprite_display_data_t* sprite_display_section; /* 0x204 */
    battle_unit_sprite_block_t sprite_blocks[3];              /* 0x208..0x297 */
    u8 shadow_graphic_trigger;                                /* 0x298; set_/unset_shadow_graphic_trigger_by_misc_id */
    u8 shadow_dirty;        /* 0x299; bit 0 set when a step lands, cleared on shadow redraw */
    u8 shadow_counter;      /* 0x29a; shadow animation counter (battle_gfx_draw_unit_shadow) */
    u8 _padding_29b;        /* aligns shadow_quad */
    SVECTOR shadow_quad[4]; /* 0x29c; shadow corners written by battle_gfx_calculate_sprite_shadow_from_tile_slope */
    /*
     * Numerical display / status text (0x2bc..0x2d8).
     * prep_for_displaying_earned_exp_jp and activate_numerical_sprite_data test
     * the activation flag, zero the animation progress and select FRAME.BIN in
     * the three numeric battle_gfx_sprite_display_data_t slots.
     */
    u8 numeric_display_active;                             /* 0x2bc */
    u8 _padding_2bd;                                       /* aligns numeric_display_selector */
    u16 numeric_display_selector;                          /* 0x2be; battle_numeric_display_selector_e */
    u16 numeric_display_value;                             /* 0x2c0; number to display */
    s16 numeric_display_progress;                          /* 0x2c2; animation progress, capped at 0x15 */
    battle_gfx_sprite_display_data_t* numeric_displays[3]; /* 0x2c4, 0x2c8, 0x2cc */
    u8 item_ability_display;                               /* 0x2d0; item ability display (byte store) */
    u8 _unused_2d1;                                        /* 0x2d1 */
    s8 item_get_x_offset;                                  /* 0x2d2; setup_item_get_rendering reads (s8) */
    s8 item_get_y_offset;                                  /* 0x2d3 */
    u8 _unused_2d4[4];
    battle_gfx_sprite_display_data_t* item_display; /* 0x2d8; item_t/Equip display pointer */
    /* Status bubble (0x2dc..0x2e7);
     * battle_gfx_update_status_bubble_graphic_trigger sets the flag and clears
     * the timer with a halfword store, so the timer is declared u16. */
    u8 status_bubble_active;         /* 0x2dc */
    u8 status_bubble_id;             /* 0x2dd */
    u8 status_bubble_x;              /* 0x2de */
    u8 status_bubble_y;              /* 0x2df */
    u16 status_bubble_timer;         /* 0x2e0 */
    u16 status_bubble_alternate_row; /* 0x2e2; non-zero selects the second texture row */
    void* status_bubble_display;     /* 0x2e4; pointer into the 0x410 status bubble data */
    /* 0x2e8; enables the six-copy position trail seeded by
     * battle_gfx_init_position_vector_copies. */
    u32 position_copies_active;
    /* Six vectors used during sprite rotation/scaling render (0x2ec);
     * battle_gfx_init_position_vector_copies seeds all six from screen_x/z/y. */
    SVECTOR display_svectors[6]; /* 0x2ec..0x31b */
    u8 _unused_31c[0x124];       /* 0x31c..0x43f: sprite display sections */
} battle_unit_misc_data_t;
typedef char battle_misc_data_size_must_be_0x440[(sizeof(battle_unit_misc_data_t) == 0x440) ? 1 : -1];
typedef char
    battle_misc_step_speed_must_be_0x03c[((unsigned long)&((battle_unit_misc_data_t*)0)->step_speed == 0x03c) ? 1 : -1];
typedef char battle_misc_distortion_target_must_be_0x090
    [((unsigned long)&((battle_unit_misc_data_t*)0)->distortion_target == 0x090) ? 1 : -1];
typedef char battle_misc_animation_frame_must_be_0x1e0
    [((unsigned long)&((battle_unit_misc_data_t*)0)->animation_frame == 0x1e0) ? 1 : -1];
typedef char battle_misc_animation_flags_must_be_0x1f0
    [((unsigned long)&((battle_unit_misc_data_t*)0)->animation_flags == 0x1f0) ? 1 : -1];
typedef char battle_misc_sprite_blocks_must_be_0x208
    [((unsigned long)&((battle_unit_misc_data_t*)0)->sprite_blocks == 0x208) ? 1 : -1];
typedef char
    battle_misc_shadow_must_be_0x299[((unsigned long)&((battle_unit_misc_data_t*)0)->shadow_dirty == 0x299) ? 1 : -1];
typedef char
    battle_misc_command_must_be_0x158[((unsigned long)&((battle_unit_misc_data_t*)0)->command_state == 0x158) ? 1 : -1];
typedef char battle_misc_facing_hint_must_be_0x164
    [((unsigned long)&((battle_unit_misc_data_t*)0)->command_state.cursor.facing_hint == 0x164) ? 1 : -1];
typedef char battle_misc_target_x_must_be_0x168
    [((unsigned long)&((battle_unit_misc_data_t*)0)->command_state.cursor.target_panel == 0x168) ? 1 : -1];

extern u16 g_battle_screen_fade_intensity;
extern POLY_F4 g_battle_screen_fade_polygons[2];
extern DR_MODE g_battle_screen_fade_draw_modes[];
extern s32 g_battle_screen_fade;

s32 battle_calculate_screen_z_from_input_coords(
    struct battle_unit_misc_data* unit, const battle_screen_coords_t* coords);

/* camera */
/* Script-variable word indices that mirror the live camera. Proven by
 * battle_camera_store_state_to_script_variables (0x80143a9c), which writes
 * them, and battle_camera_load_state_from_script_variables, which reads them back into a VECTOR
 * (X -> vx, Z (height) -> vy, Y (map depth) -> vz). */
enum {
    BATTLE_CAMERA_X_WORD_INDEX = 0x1a,
    BATTLE_CAMERA_Z_WORD_INDEX = 0x1b,
    BATTLE_CAMERA_Y_WORD_INDEX = 0x1c,
    BATTLE_CAMERA_TILT_WORD_INDEX = 0x1d,
    BATTLE_CAMERA_YAW_WORD_INDEX = 0x1e,
    BATTLE_CAMERA_ROLL_WORD_INDEX = 0x1f,
    BATTLE_CAMERA_ZOOM_WORD_INDEX = 0x20,
};

/* Camera rotation angles: an unsigned view of an SVECTOR's first three
 * halfwords (the target loads them with lhu). */
typedef struct battle_camera_rotation {
    u16 x;
    u16 y;
    u16 z;
} battle_camera_rotation_t;
typedef char battle_camera_rotation_size_must_be_6[sizeof(battle_camera_rotation_t) == 6 ? 1 : -1];

/* Provisional camera interpolation key: frame time and target value. */
typedef struct battle_camera_key {
    s32 time;  /* 0x00; cumulative frame of this key */
    s32 value; /* 0x04 */
    u8 _unused_08[8];
} battle_camera_key_t;

/* Provisional per-component camera fusion track (0xa4 bytes). */
typedef struct battle_camera_track {
    battle_camera_key_t keys[8]; /* 0x00 */
    s32 key_count;               /* 0x80 */
    s32 segment;                 /* 0x84; first key of the active segment */
    s32 frame;                   /* 0x88; frames since the track started */
    s32 segment_frame;           /* 0x8c; frames into the active segment */
    s32 prev_time;               /* 0x90; Q12 */
    s32 prev_value;              /* 0x94; Q12 */
    s32 start_value;             /* 0x98; Q12 */
    s32 segment_duration;        /* 0x9c */
    s32 finished;                /* 0xa0 */
} battle_camera_track_t;
typedef char battle_camera_track_size_must_be_0xa4[sizeof(battle_camera_track_t) == 0xa4 ? 1 : -1];

extern u16 g_battle_camera_shake_z_offset;
extern s32 g_battle_previous_camera_yaw;
extern u16 g_battle_camera_script_variable_indices[7];
extern s16 g_battle_camera_position_locked;
extern s32 g_battle_camera_yaw_band;
extern s32 g_battle_camera_wrap_yaw_pending;
extern VECTOR g_battle_camera_script_vector;
extern void* g_battle_camera_published_coords_ptr;
extern battle_camera_rotation_t g_battle_camera_script_rotation;
extern SVECTOR* g_battle_camera_published_rotation_ptr;
/* Maps a camera quadrant index to its map-tile camera-block direction. */
extern u8 g_battle_camera_quadrant_block_directions[4];
extern VECTOR g_battle_camera_current_real_coords;
extern SVECTOR g_battle_camera_integer_coords;
extern MATRIX g_battle_camera_matrix;
extern u16 g_battle_camera_offset_screen_coord_countdown;
extern VECTOR g_battle_camera_offset_screen_coords;
extern VECTOR g_battle_camera_offset_screen_coords_fixed;
extern u16 g_battle_camera_real_coord_countdown;
extern VECTOR g_battle_camera_real_coord_delta;
extern s32 g_battle_camera_rotation_action;
extern SVECTOR g_battle_camera_rotation_backup;
extern u16 g_battle_camera_rotation_countdown;
extern SVECTOR g_battle_camera_rotation_delta;
extern s32 g_battle_camera_rotation_increment;
extern VECTOR g_battle_camera_saved_real_coords;
extern s32 g_battle_camera_tilt_action;
extern s32 g_battle_camera_tilt_increment;
extern VECTOR g_battle_camera_zoom;
extern s32 g_battle_camera_zoom_action;
extern VECTOR g_battle_camera_zoom_backup;
extern u16 g_battle_camera_zoom_countdown;
extern VECTOR g_battle_camera_zoom_delta;
extern s32 g_battle_camera_zoom_increment;
extern s32 g_battle_camera_cross_product_overflow; /* set by battle_fixed_cross_product_q12 on Q12 overflow */
extern s16 g_battle_camera_diagonal_yaws[4];

/* Base of the large render-state records; low offsets overlap camera/GPU fields.
 * .vx is the camera pitch and .vy its yaw. */
extern SVECTOR g_battle_camera_render_state;

/* 0x80166054: camera consumer unpacks two 2-bit fields and a high nibble. */
extern s32 g_battle_camera_speed_curve;
void battle_camera_convert_screen_coords_by_rotation(const u16* input, s16* output, s32 offset);
void battle_camera_convert_screen_coords_modify_by_1(const s16* screen, s16* destination);
void battle_camera_toggle_tilt(s32 unused_0, s32 sound_arg);
s16 battle_camera_pick_nearest_unblocked_yaw(s32 misc_id_a, s32 misc_id_b);
void battle_camera_step_focus_toward_cursor_tile(s16 height);
void battle_camera_execute_angle_command(s32 selector, s32 frame_high, s32 duration, SVECTOR* position);
void battle_camera_execute_position_command(s32 selector, s32 frame_high, s32 duration, VECTOR* target);
s32 battle_camera_get_input_direction(s32 mode, u32 input);
void battle_camera_update_effect_rotation_and_zoom(void);

/* Some routines address these components as separate linker symbols. */
void battle_camera_update_vector_from_game_state(void);
void battle_camera_calculate_acting_unit_vector_normal(void);
s32 battle_camera_calculate_cursor_tile_vector_normal(void);
void battle_camera_call_rotate_left(s32 target_angle);
void battle_camera_call_rotate_right(s32 target_angle);
void battle_camera_call_toggle_tilt(void);
void battle_camera_call_zoom_map(void);
void battle_camera_copy_current_real_coords(void);
void battle_camera_copy_rotation(void);
void battle_camera_copy_zoom(void);
void battle_camera_execute_zoom_command(s32 flags, s32 unused, s32 duration, VECTOR* zoom);
VECTOR* battle_camera_get_current_real_coords(void);
u16* battle_camera_get_rotation(void);
VECTOR* battle_camera_get_zoom(void);
void battle_camera_handle_rotation_input(void);
void battle_camera_init_defaults(void);
void battle_camera_init_tilt_and_zoom(void);
void battle_camera_normalize_yaw_angle(void);
s32 battle_camera_is_active(void);
void battle_camera_project_world_coords_to_screen(const battle_screen_coords_t* world_coords, s16* screen_coords);
void battle_camera_request_rotation(s32 angle);
void battle_camera_reset_script_transform(void);
void battle_camera_restore_current_real_coords(void);
void battle_camera_restore_rotation(void);
void battle_camera_restore_zoom(void);
void battle_camera_rotate_left(s32 target_angle);
void battle_camera_rotate_right(s32 target_angle);
void battle_camera_rotate_when_unit_tile_not_visible(struct battle_unit_misc_data* unit);
s32 battle_camera_scale_cursor_tile_vector(void);
void battle_camera_set_current_real_coords(const VECTOR* coords);
void battle_camera_set_rotation(const battle_camera_rotation_t* rotation);
void battle_camera_set_zoom(const VECTOR* zoom);
s32 battle_camera_step_track(battle_camera_track_t* track, s32 component);
void battle_camera_store_state_to_script_variables(void);
void battle_camera_store_yaw_band(s32 value);
void battle_camera_toggle_zoom(void);
void battle_camera_update_map_pan(void);

void battle_camera_update_matrices(
    MATRIX* camera_matrix, void* render_buffers, void* offset_screen_coords, VECTOR* camera_zoom);

void battle_camera_update_offset_screen_coord_animation(void);
void battle_camera_update_real_coord_animation(void);
void battle_camera_update_rotation(void);
void battle_camera_update_rotation_animation(void);
void battle_camera_update_tilt(void);
void battle_camera_update_zoom(void);
void battle_camera_update_zoom_animation(void);
s32 battle_camera_wrap_yaw_angle(void);
void battle_camera_advance_effect_tracks(s16 frame);
void battle_camera_calculate_relative_offset(const u16* input, s16* output, s32 offset);
void battle_camera_calculate_relative_offset_1(const u16* input, s16* output);
void battle_camera_calculate_relative_offset_5(const u16* input, s16* output);

void battle_camera_choose_unblocked_rotation_for_tile(
    const battle_screen_coords_t* tile_coords, battle_camera_rotation_t* rotation_out);

void battle_camera_convert_screen_coords_modify_by_5(const s16* screen, s16* destination);
s32 battle_camera_find_active_keyframe(s16 frame, s32 mask, s32* out_index, s32* out_frame);
void battle_camera_load_state_from_script_variables(void);

void battle_camera_set_focus_and_cursor_tile(
    const battle_screen_coords_t* coords, const battle_camera_rotation_t* rotation);

void battle_camera_start_effect_keyframe_moves(s16 frame);
void battle_camera_update_cursor_tile_vector(void);
void battle_camera_shift_acting_unit_vector(void);
void battle_camera_fusion_thread(void);
void battle_camera_wait_until_idle(void);
void battle_camera_thread(void);

/* sound */
/* SEQ animation control opcodes, read after a BATTLE_SEQ_OP_PREFIX byte by
 * battle_gfx_run_unit_seq_script and battle_gfx_run_wep_eff_seq_script.
 * UNKNOWN_XX commands have no known effect beyond their operand skip. For
 * 0xf0 (MoveUnitRL) the unit interpreter calls battle_unit_shift_right_left. */
typedef enum battle_seq_opcode {
    BATTLE_SEQ_OP_PREFIX = 0xff,
    BATTLE_SEQ_OP_UNKNOWN_BE = 0xbe,
    BATTLE_SEQ_OP_UNKNOWN_BF = 0xbf,
    BATTLE_SEQ_OP_WAIT_FOR_DISTORT = 0xc0,
    BATTLE_SEQ_OP_QUEUE_DISTORT_ANIM = 0xc1,
    BATTLE_SEQ_OP_UNKNOWN_C2 = 0xc2,
    BATTLE_SEQ_OP_UNLOAD_MF_ITEM = 0xc3,
    BATTLE_SEQ_OP_MF_ITEM_POS = 0xc4,
    BATTLE_SEQ_OP_LOAD_MF_ITEM = 0xc5,
    BATTLE_SEQ_OP_WAIT_FOR_INPUT = 0xc6,
    BATTLE_SEQ_OP_UNKNOWN_C7 = 0xc7,
    BATTLE_SEQ_OP_UNKNOWN_C8 = 0xc8,
    BATTLE_SEQ_OP_UNKNOWN_C9 = 0xc9,
    BATTLE_SEQ_OP_UNKNOWN_CA = 0xca,
    BATTLE_SEQ_OP_MOVE_UP_2 = 0xcb,
    BATTLE_SEQ_OP_MOVE_UP_1 = 0xcc,
    BATTLE_SEQ_OP_MOVE_BACKWARD_2 = 0xcd,
    BATTLE_SEQ_OP_MOVE_BACKWARD_1 = 0xce,
    BATTLE_SEQ_OP_MOVE_DOWN_2 = 0xcf,
    BATTLE_SEQ_OP_MOVE_DOWN_1 = 0xd0,
    BATTLE_SEQ_OP_MOVE_FORWARD_2 = 0xd1,
    BATTLE_SEQ_OP_MOVE_FORWARD_1 = 0xd2,
    BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_1 = 0xd3,
    BATTLE_SEQ_OP_PLAY_ATTACK_SOUND = 0xd4,
    BATTLE_SEQ_OP_INCREMENT_LOOP = 0xd5,
    BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_2 = 0xd6,
    BATTLE_SEQ_OP_UNKNOWN_D7 = 0xd7,
    BATTLE_SEQ_OP_SET_FRAME_OFFSET = 0xd8,
    BATTLE_SEQ_OP_QUEUE_THROW_ANIMATION = 0xd9,
    BATTLE_SEQ_OP_RETURN_ERROR_FINISH_ANIM = 0xda,
    BATTLE_SEQ_OP_SET_SLOWDOWN = 0xdb,
    BATTLE_SEQ_OP_RELOAD_ANIMATION = 0xdc,
    BATTLE_SEQ_OP_OVERRIDE_ANIMATION = 0xdd,
    BATTLE_SEQ_OP_POST_GENERIC_ATTACK = 0xde,
    BATTLE_SEQ_OP_SET_Y_ROTATION_0 = 0xdf,
    BATTLE_SEQ_OP_CLEAR_SHADOW = 0xe0,
    BATTLE_SEQ_OP_SET_SHADOW = 0xe1,
    BATTLE_SEQ_OP_SET_LAYER_PRIORITY = 0xe2,
    BATTLE_SEQ_OP_UNKNOWN_E3 = 0xe3,
    BATTLE_SEQ_OP_UNKNOWN_E4 = 0xe4,
    BATTLE_SEQ_OP_SAVE_Y_SPIN = 0xe5,
    BATTLE_SEQ_OP_UNKNOWN_E6 = 0xe6,
    BATTLE_SEQ_OP_UNKNOWN_E7 = 0xe7,
    BATTLE_SEQ_OP_UNKNOWN_E8 = 0xe8,
    BATTLE_SEQ_OP_UNKNOWN_E9 = 0xe9,
    BATTLE_SEQ_OP_UNKNOWN_EA = 0xea,
    BATTLE_SEQ_OP_FLIP_VERTICAL = 0xeb,
    BATTLE_SEQ_OP_FLIP_HORIZONTAL = 0xec,
    BATTLE_SEQ_OP_UNKNOWN_ED = 0xed,
    BATTLE_SEQ_OP_MOVE_UNIT_FB = 0xee,
    BATTLE_SEQ_OP_MOVE_UNIT_DU = 0xef,
    BATTLE_SEQ_OP_MOVE_UNIT_RL = 0xf0,
    BATTLE_SEQ_OP_UNKNOWN_F1 = 0xf1,
    BATTLE_SEQ_OP_QUEUE_SPRITE_ANIM = 0xf2,
    BATTLE_SEQ_OP_UNKNOWN_F3 = 0xf3,
    BATTLE_SEQ_OP_UNKNOWN_F4 = 0xf4,
    BATTLE_SEQ_OP_UNKNOWN_F5 = 0xf5,
    BATTLE_SEQ_OP_PLAY_SOUND = 0xf6,
    BATTLE_SEQ_OP_UNKNOWN_F7 = 0xf7,
    BATTLE_SEQ_OP_UNKNOWN_F8 = 0xf8,
    BATTLE_SEQ_OP_UNKNOWN_F9 = 0xf9,
    BATTLE_SEQ_OP_MOVE_UNIT = 0xfa,
    BATTLE_SEQ_OP_UNKNOWN_FB = 0xfb,
    BATTLE_SEQ_OP_WAIT = 0xfc,
    BATTLE_SEQ_OP_HOLD_WEAPON = 0xfd,
    BATTLE_SEQ_OP_END_ANIMATION = 0xfe,
    BATTLE_SEQ_OP_PAUSE_ANIMATION = 0xff,
} battle_seq_opcode_e;

extern s32 g_battle_sound_primary_weather_sfx_id;
extern s32 g_battle_sound_secondary_weather_sfx_id;
extern s32 g_battle_sound_suppressed;
extern s32 g_battle_sound_tertiary_weather_sfx_id;
extern u8 g_battle_sound_weather_sfx_enabled;
extern s32 g_sound_effect_id_to_play;
extern u8 g_battle_sound_weapon_miss_sfx_ids[];
extern u8 g_battle_sound_weapon_hit_sfx_ids[];
extern u8 g_battle_sound_shield_deflect_sfx_ids[];
extern s16 g_battle_music_set_track_request;

/* SwitchTrack's first operand is a trigger, not the selected track ID.
 * The consumer toggles the zero-based index, then uses main music slot 1/2. */
extern s16 g_battle_music_switch_request; /* 0x80165fc8 */

/* u16: the only loads of these three in the tree are in
 * attack_sound_resync_scenario_music_and_apply_map_darkness, which the target
 * reaches with lhu (that file's own comment records the andi a u16 merge would
 * add). Every other use is a store, which is sign-agnostic. */
extern u16 g_battle_music_track_1_id;          /* 0x80165fd4 */
extern u16 g_battle_music_track_2_id;          /* 0x80165fd6 */
extern s16 g_battle_music_unload_slot_request; /* 0x80165fca */

/* 0x80165fc0: -1 absent; volume = low 16 bits,
 * duration = (packed >> 14) & 0x3ffc. Not a linear duration field. */
extern s32 g_battle_music_volume_transition_request;
void battle_sound_play_movement_sfx(battle_unit_misc_data_t* unit, s32 sound_id);
void battle_sound_play_music_tracks(s32 first_track, s32 second_track);
void battle_sound_select_weapon_hit(battle_unit_misc_data_t* unit, s32 hit);
void battle_sound_set_effect_to_cancel(void);
void battle_sound_set_effect_to_confirm(void);
void battle_sound_set_effect_to_confirm_checked(void);
void battle_sound_set_effect_to_cursor_move(void);
void battle_sound_set_effect_to_invalid(void);
void battle_sound_start_weather_sfx(void);
s32 battle_sound_stop_weather_sfx(void);
void battle_sound_wait_for_music_stub(void);
void battle_load_seq_data(u32* destination, const u8* source);
void battle_sound_bg_thread(void);
void battle_sound_edit_bg_thread(void);

/* effect */
enum { BATTLE_EFFECT_FLAGS_0006 = 0x0006 };

/* Verified prefix only: BATTLE 0x801aa7c4 reads/writes a halfword at +6.
 * Neither the full record nor the individual bit meanings are known. */
typedef struct battle_effect_flag_prefix {
    u8 _unused_00[6];
    u16 flags;
} battle_effect_flag_prefix_t;
typedef char
    battle_effect_flag_prefix_flags_must_be_6[((unsigned long)&((battle_effect_flag_prefix_t*)0)->flags == 6) ? 1 : -1];

/* Ten-byte halfword block the secondary-effect initializers copy from
 * battle_effect_secondary_init_t into battle_effect_secondary_data_t. */
typedef struct battle_effect_secondary_block {
    s16 values[5];
} battle_effect_secondary_block_t;

/* Secondary-effect target slots at 0x801b8b9c, stride 0x54. The matching phase
 * accessors establish phase at +0x08; battle_effect_update_secondary_effects
 * reads +0x00/+0x03. battle_effect_allocate_secondary_slot links slots through
 * +0x00/+0x01 and clears the 0x20-byte work area at +0x30. The initializers
 * (battle_effect_init_secondary and its projectile/fall-dust variants) fill
 * +0x04..+0x2b. Ending setup at 0x801ae2c8 stores halfwords at +0x04/+0x0c;
 * the golem-family handlers use +0x0c as their counter. Unused regions below
 * are deliberately unnamed. */
typedef struct battle_effect_secondary_data {
    u8 previous_slot_id; /* 0x00 */
    u8 next_slot_id;     /* 0x01 */
    u8 own_slot_id;      /* 0x02; index of this slot in g_battle_effect_secondary_data */
    u8 function_id;      /* 0x03 */
    u16 animation_id;    /* 0x04 */
    s16 parameter;       /* 0x06; caster sprite palette, or the projectile animation's high half */
    s32 phase;           /* 0x08 */
    s16 timer;           /* 0x0c */
    u8 _unused_0e[2];
    battle_effect_secondary_block_t caster_block; /* 0x10; from the init record's caster block */
    union {
        battle_effect_secondary_block_t block; /* from the init record's target block */
        struct {
            s16 _unused_00;
            s16 x; /* 0x1c; fall-dust position, 1/16 of the real coordinate */
            s16 z; /* 0x1e */
            s16 y; /* 0x20 */
        } position;
        struct {
            s16 target_type;   /* 0 identifies a unit target (battle_effect_target_t) */
            u8 target_id;      /* 0x1c; read by the Splash handler 0x801b2a88 */
            u8 target_byte_1d; /* 0x1d; the arrow and projectile handlers compare it with 3 and 4..5 */
        } fields;
    } target;           /* 0x1a */
    s32 used_weapon_id; /* 0x24; init record used_weapon_id */
    s32 target_count;   /* 0x28; init record target_count (projectile setup) */
    /* 0x2c; slots started by battle_effect_start_group, decremented as they
     * finish (0x801b0cf0); handlers finish at 0. */
    s16 active_count;
    s16 finish_timer;  /* 0x2e; counts frames after the last slot finished */
    u8 slot_ids[0x20]; /* 0x30; effect slots started for this effect; cleared when the slot is linked */
    void* allocation;  /* 0x50; freed and cleared when the slot is removed (0x801ad944) */
} battle_effect_secondary_data_t;
typedef char battle_secondary_effect_data_size_must_be_0x54[(sizeof(battle_effect_secondary_data_t) == 0x54) ? 1 : -1];

/* Ten-byte target records at g_battle_effect_coord_data.targets (0x801bad10).
 * Unit targets store a misc id at 0x02; tile targets reuse that halfword for X
 * and carry map Z/Y at 0x04/0x06. Entry 16 is the caster. */
typedef struct battle_effect_target {
    u16 target_type; /* 0x00; 0 identifies a unit target */
    union {
        u8 misc_id;
        s16 tile_x;
        u8 bytes[2];  /* bytes[1]: 2 selects the child timeline's secondary-effect path */
    } id;             /* 0x02 */
    s16 map_z;        /* 0x04 */
    s16 tile_y;       /* 0x06 */
    u8 _unused_08[2]; /* 0x08 */
} battle_effect_target_t;
typedef char battle_effect_target_size_must_be_0x0a[(sizeof(battle_effect_target_t) == 0x0a) ? 1 : -1];

/* 0xc8-byte stack record filled by battle_effect_build_secondary_init and
 * read by the secondary-effect initializers, which copy its two ten-byte
 * blocks whole. */
typedef struct battle_effect_secondary_init {
    u16 target_count;        /* 0x00 */
    u8 palette_target_count; /* 0x02; entries in palette_target_ids */
    u8 math_skill_flag;      /* 0x03; strike work control_value_19f */
    union {
        struct {
            s16 target_type; /* 0x04; 0 unit, 1 tile (battle_effect_target_t) */
            u8 target_id;    /* 0x06 */
            /* 0x07; 0 normal, 1 critical, 3..5 by action miss_type, 6 break
             * (set per target by battle_effect_build_secondary_init_from_action) */
            u8 result_animation;
        } fields;
        battle_effect_secondary_block_t block;
    } target; /* 0x04; first of up to 16 ten-byte target blocks */
    u8 _unused_0e[0x96];
    union {
        struct {
            s16 target_type; /* 0xa4; copied whole from g_battle_effect_targets[16] */
            u8 caster_id;    /* 0xa6 */
        } fields;
        battle_effect_secondary_block_t block;
    } caster; /* 0xa4 */
    /* 0xae; living units that are neither targets nor the caster; copied to
     * g_battle_effect_palette_target_misc_ids. */
    u8 palette_target_ids[14];
    battle_effect_target_t target_tile; /* 0xbc; post-action destination tile */
    u16 used_weapon_id;                 /* 0xc6 */
} battle_effect_secondary_init_t;
typedef char battle_secondary_effect_init_size_must_be_0xc8[(sizeof(battle_effect_secondary_init_t) == 0xc8) ? 1 : -1];

/* Four-byte records at 0x801b84dc indexed by animation id; byte 0 selects the
 * g_battle_effect_secondary_handlers handler. */
typedef struct battle_effect_secondary_animation {
    u8 function_id; /* 0x00 */
    u8 _padding_01; /* 0x01; aligns parameter */
    s16 parameter;  /* 0x02; lh 0x801b84de[id * 4], read by the arrow/projectile handlers */
} battle_effect_secondary_animation_t;

/* Queue of post-action "effect message" popups, drained newest-first by
 * battle_action_resume_attack_phase_control. The stride is 8: the producer at
 * 0x80072d8c indexes the array directly and the consumer walks it by
 * `idx * 8`, and only these three fields are ever read or written. */
typedef struct battle_post_effect_msg {
    u8 code;           /* 0x00 message code passed to battle_menu_init_system_function */
    u8 unit;           /* 0x01 misc ID of the acting unit */
    u8 _padding_02[2]; /* 0x02; aligns value */
    s32 value;         /* 0x04 reported amount */
} battle_post_effect_msg_t;

/* Effect file preamble and offsets to its three following sections. */
typedef struct battle_effect_resource {
    u8 _unused_00[0xc];
    s32 words[6];
} battle_effect_resource_t;

/* Twelve-byte on-hit effect vector at on_hit_effects_data (0x801b8a40), with a
 * saved copy at on_hit_effects_data_second_section (0x801b8a50). The
 * secondary-effect handlers store (0, 0x1000, 0) before stepping the effect
 * slots; effect script 0x27 copies it from the particle-system data. */
typedef struct battle_effect_on_hit_vector {
    s32 x;
    s32 y;
    s32 z;
} battle_effect_on_hit_vector_t;

/* Per-group effect records at 0x801b8566, stride 0x2e. The secondary-effect
 * handlers spawn while spawn_start_frame <= timer < spawn_end_frame and finish
 * once timer passes spawn_start_frame with no live slots;
 * battle_effect_update_active_slots walks count active slots. */
typedef struct battle_effect_group {
    u8 spawn_start_frame; /* 0x00 */
    u8 spawn_end_frame;   /* 0x01 */
    u8 count;             /* 0x02 */
    u8 _unused_03[7];
    /* 0x0a; emitter position[1] (vertical spawn offset); the level-up handler
     * seeds group 5's with -8, then lowers it by 3 per frame. */
    s16 position_y;
    u8 _unused_0c[0x1c];
    u8 spawns_per_frame; /* 0x28; slots one frame may start */
    u8 _unused_29[5];
} battle_effect_group_t;
typedef char battle_effect_group_size_must_be_0x2e[(sizeof(battle_effect_group_t) == 0x2e) ? 1 : -1];

/* Tile target of the current effect: placement mode 0x200, emitter modes 0x40/0xa0 and camera mode 0x1c0. */
/* Provisional view of an effect slot's position and velocity words. */
typedef struct battle_effect_charge_trail {
    DVECTOR points[7]; /* 0x00; screen positions, newest at age % 7 */
    s16 age;           /* 0x1c */
    u8 slot_id;        /* 0x1e */
    u8 _padding_1f;    /* tail padding to 2-byte alignment */
} battle_effect_charge_trail_t;

typedef struct battle_effect_charge_state {
    s16 x;                                   /* 0x0000 */
    s16 y;                                   /* 0x0002 */
    DR_MODE mode[2];                         /* 0x0004 */
    LINE_G2 lines[2][96];                    /* 0x001c */
    battle_effect_charge_trail_t trails[16]; /* 0x0f1c */
    u8 ids_a[2];                             /* 0x111c */
    u8 ids_b[14];                            /* 0x111e */
    union {
        struct {
            s16 trails;
            u8 a;
            u8 b;
        } n;
        s32 all;
    } count;            /* 0x112c */
    s32 angle;          /* 0x1130 */
    s32 speed;          /* 0x1134 */
    s32 spread_range;   /* 0x1138 */
    s32 spread_offset;  /* 0x113c */
    s32 duration;       /* 0x1140 */
    s32 radius;         /* 0x1144 */
    s32 spawn_interval; /* 0x1148 */
    s32 colour;         /* 0x114c */
    s32 max_trails;     /* 0x1150 */
    s32 fade_row;       /* 0x1154 */
    s32 done;           /* 0x1158 */
} battle_effect_charge_state_t;

/* Provisional summon-charge state: three rings of ten trailing points. */
typedef struct battle_effect_ring_state {
    s32 index;             /* 0x00; newest trail column, 0..9 */
    s32 radius;            /* 0x04 */
    s32 angle;             /* 0x08 */
    s32 angle_step;        /* 0x0c */
    SVECTOR center;        /* 0x10 */
    s32 brightness;        /* 0x18 */
    SVECTOR points[3][10]; /* 0x1c */
} battle_effect_ring_state_t;

/* Provisional zodiac-poof spawn state for the 24-slot trap group 3. */
typedef struct battle_effect_trap_state_24 {
    s32 done;   /* 0x00; reaches 1 when the effect is finished */
    s32 count;  /* 0x04; slots started so far */
    u8 ids[24]; /* 0x08 */
} battle_effect_trap_state_24_t;

/* Work area at 0x801bade0 shared by the secondary effect handlers; the
 * spell-charge, summon-charge and zodiac-poof handlers each lay it out as
 * their own state. */
typedef union battle_effect_secondary_state {
    battle_effect_charge_state_t charge;
    battle_effect_ring_state_t summon_ring;
    battle_effect_trap_state_24_t zodiac_poof;
} battle_effect_secondary_state_t;

typedef struct battle_effect_coord_data {
    u16 hit_counter;                    /* 0x00; effect target count: bounds child spawns and scales spawn_delay */
    u8 palette_target_count;            /* 0x02; entries of g_battle_effect_palette_target_misc_ids */
    u8 math_skill_flag;                 /* 0x03; enables the effect flags' spawn_delay_override */
    battle_effect_target_t targets[17]; /* 0x04 */
} battle_effect_coord_data_t;

/* 0x80-byte particle-channel keyframe table walked by
 * battle_effect_step_emitter_timeline. The halfword arrays at 0x00 and 0x4a
 * are indexed by keyframe, the byte array at 0x31 selects the emitter action,
 * and 0x7e is the highest valid keyframe index (the role
 * effect_misc_data_t::last_keyframe_index plays). Array bounds are inferred
 * from the gaps between the observed offsets. */
typedef struct battle_effect_keyframe_table {
    u16 frame_start[0x18]; /* 0x00 */
    u8 _unused_30;         /* 0x30 */
    u8 action[0x19];       /* 0x31 */
    u16 selector[0x18];    /* 0x4a */
    u8 _unused_7a[4];      /* 0x7a */
    s16 last_index;        /* 0x7e */
} battle_effect_keyframe_table_t;
typedef char battle_effect_keyframe_table_size_must_be_0x80[(sizeof(battle_effect_keyframe_table_t) == 0x80) ? 1 : -1];

enum {
    BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES = 33,
};

/* Timed map background changes: the palette-track layout with a second
 * colour per keyframe for the lower gradient stop. */
typedef struct battle_effect_background_track {
    s16 duration[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s8 color[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES][3];
    s8 second_color[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES][3];
    u8 preset[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} battle_effect_background_track_t;
typedef char
    battle_effect_background_track_size_must_be_0x12c[(sizeof(battle_effect_background_track_t) == 0x12c) ? 1 : -1];

/* Timed palette changes used by caster and target effect tracks. */
typedef struct battle_effect_palette_track {
    s16 duration[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s8 color[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES][3];
    u8 preset[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} battle_effect_palette_track_t;
typedef char battle_effect_palette_track_size_must_be_0xc8[sizeof(battle_effect_palette_track_t) == 0xC8 ? 1 : -1];

/* One phase's colour tracks. */
typedef struct effect_timeline_color_tracks {
    battle_effect_palette_track_t affected_units; /* 0x000 */
    battle_effect_palette_track_t caster;         /* 0x0c8 */
    battle_effect_palette_track_t target;         /* 0x190 */
    battle_effect_background_track_t screen;      /* 0x258 */
} effect_timeline_color_tracks_t;

/* Single-phase timeline channels beginning eight bytes into the effect's
 * timing section. The multi-phase effect_misc_data_t view has a different
 * layout after its common header. */
typedef struct battle_effect_tick_sound_track {
    u16 duration[17]; /* 0x00 */
    u8 kind[17];      /* 0x22; 0/1 none, 2+ on-hit sound index + 2 */
    u8 _padding_33;   /* aligns count */
    s16 count;        /* 0x34 */
} battle_effect_tick_sound_track_t;

typedef struct battle_effect_tick_channels {
    u16 _unused_000;
    u16 duration;                                 /* 0x002 */
    battle_effect_keyframe_table_t particle[5];   /* 0x004 */
    battle_effect_tick_sound_track_t sound[3];    /* 0x284 */
    battle_effect_palette_track_t affected_units; /* 0x326 */
    battle_effect_palette_track_t caster;         /* 0x3ee */
    battle_effect_palette_track_t target;         /* 0x4b6 */
    battle_effect_background_track_t screen;      /* 0x57e */
} battle_effect_tick_channels_t;

/* Provisional view of the interpolated emitter values the Emitter Control
 * Routine passes in (0x801c878c). The secondary handlers pass a group record's
 * address minus two bytes (a raw byte offset: this view and
 * battle_effect_group_t are not yet reconciled). */
typedef struct battle_effect_emitter_values {
    u8 _unused_00[6];    /* 0x00 */
    u16 anchor_flags;    /* 0x06; 0xe00 selects the spawn anchor */
    u16 motion_flags;    /* 0x08; 0x410 selects the orientation mode */
    s16 position[3];     /* 0x0a */
    s16 spread[3];       /* 0x10 */
    s16 angle[3];        /* 0x16 */
    u16 angle_spread[3]; /* 0x1c */
    s16 weight_min;      /* 0x22 */
    s16 weight_max;      /* 0x24 */
    u16 speed_min;       /* 0x26 */
    u16 speed_max;       /* 0x28 */
    u8 _unused_2a[2];    /* 0x2a */
    s8 lifetime_min;     /* 0x2c */
    s8 lifetime_max;     /* 0x2d */
} battle_effect_emitter_values_t;

/* One textured quad of a multi-part effect sprite. */
typedef struct battle_effect_sprite_part {
    u16 flags; /* 0x00; 0x0f CLUT row, 0x10 upper CLUT bank, 0x200 semi-transparent */
    u16 tpage; /* 0x02 */
    u8 u;
    u8 v;
    u8 width;
    u8 height;
    s32 corners[4]; /* 0x08; packed x/y of the four quad corners */
} battle_effect_sprite_part_t;
typedef char battle_effect_sprite_part_size_must_be_0x18[(sizeof(battle_effect_sprite_part_t) == 0x18) ? 1 : -1];

/* Provisional arc descriptor handed to battle_effect_trace_arc_trajectory_path. */
typedef struct battle_effect_arc {
    s16 _unused_00; /* 0x00 */
    u16 yaw;        /* 0x02 */
    s16 pitch;      /* 0x04 */
} battle_effect_arc_t;

/* Arc endpoint handed to the per-arc obstacle test: the unit's raised tile
 * position followed by its id and height allowance. */
typedef struct battle_arc_endpoint {
    SVECTOR position;
    s32 unit_id;
    s32 height;
} battle_arc_endpoint_t;

/* Offset to the target plus its fixed-point horizontal length. */
typedef struct battle_arc_delta {
    s32 x;
    s32 y;
    s32 z;
    s32 distance;
} battle_arc_delta_t;

/* Query read by battle_effect_calculate_arcing_trajectory: the offset to the
 * target and both endpoints. */
typedef struct battle_arc_query {
    battle_arc_delta_t delta;
    battle_arc_endpoint_t source;
    battle_arc_endpoint_t target;
} battle_arc_query_t;

/* Provisional: unit list built in the scratchpad at 0x1f800000 by
 * battle_effect_list_valid_unit_targeting_data (0x801af770) and read by the
 * obstacle tests. */
typedef struct battle_effect_obstacle_unit_list {
    s32 count;                                  /* 0x00 */
    u8 battle_ids[22];                          /* 0x04 */
    SVECTOR positions[BATTLE_UNIT_SLOT_COUNT];  /* 0x1a; tile centre and height */
    s32 sprite_heights[BATTLE_UNIT_SLOT_COUNT]; /* 0xc4 */
} battle_effect_obstacle_unit_list_t;

/* Provisional: the tile reference g_battle_effect_trajectory_tile (0x801b8b68)
 * that battle_effect_test_position_for_obstacle fills in. */
typedef struct battle_effect_tile_ref {
    s16 x;     /* 0x00 */
    s16 layer; /* 0x02 */
    s16 y;     /* 0x04 */
} battle_effect_tile_ref_t;

/* 0x1c-byte G3 polygon initialised by SetPolyG3. */
typedef struct battle_effect_g3_prim {
    u8 data[0x1c];
} battle_effect_g3_prim_t;

/* 0xc88-byte arrow work buffer of the bow-arrow (0x801b0ffc) and projectile
 * (0x801b3938) secondary handlers: two sets of four G3 and thirteen G4
 * polygons. */
typedef struct battle_effect_arrow_buffer {
    battle_effect_g3_prim_t g3[2][4]; /* 0x000 */
    POLY_G4 g4[2][13];                /* 0x0e0 */
    u8 _unused_488[0x800];
} battle_effect_arrow_buffer_t;
typedef char battle_effect_arrow_buffer_size_must_be_0xc88[(sizeof(battle_effect_arrow_buffer_t) == 0xc88) ? 1 : -1];

/* 21-keyframe camera table used by the main and cleanup timelines. */
typedef struct effect_camera_keyframes_21 {
    s16 frame_thresholds[21];
    s16 position[21][3];
    s16 target[21][3];
    s16 zoom[21][3];
    u16 selector[21];
    s16 last_keyframe_index;
} effect_camera_keyframes_21_t;

/* Effect misc data, the timeline section at +0x1c of an effect file. The
 * keyframe scanner, the timeline frame handler and the camera keyframe
 * dispatchers are views of this ONE object: the per-keyframe selector
 * halfwords sit at 0x806, and the camera position/target/zoom channels at
 * 0x6d4/0x73a/0x7a0 run exactly through what the scanner's view spelled as a
 * single field_6b2[170] threshold array. The for-each-target thresholds are
 * therefore 17 entries at 0x6b2, one per keyframe, immediately followed by the
 * three 17-entry three-halfword channels. The two phases' particle channels,
 * sound tracks and colour tracks follow, then the 21-entry main and cleanup
 * camera tables. */
typedef struct effect_misc_data {
    u16 _unused_0000;
    u16 phase2_duration; /* 0x002; effect script 0x1e adds it to the phase-2 start */
    u16 phase1_duration; /* 0x004 */
    u16 spawn_delay;     /* 0x006; frames between child spawns (see effect_flags_section_t) */
    u8 _unused_0008[2];
    u16 phase2_delay; /* 0x00a; battle_effect_find_next_keyframe also bounds its scan with it */
    u8 _unused_000c[0x6a6];
    s16 frame_thresholds[17];                               /* 0x6b2; per-keyframe frame threshold */
    s16 position[17][3];                                    /* 0x6d4; camera position channel */
    s16 target[17][3];                                      /* 0x73a; camera target channel */
    s16 zoom[17][3];                                        /* 0x7a0; camera zoom channel */
    u16 selector[17];                                       /* 0x806; per-keyframe selector mask */
    s16 last_keyframe_index;                                /* 0x828; highest valid keyframe index */
    battle_effect_keyframe_table_t particle_channels[2][5]; /* 0x82a */
    u8 sound_tracks[2][3][0x1e];                            /* 0xd2a */
    effect_timeline_color_tracks_t color_tracks[2];         /* 0xdde */
    effect_camera_keyframes_21_t main;                      /* 0x14e6 */
    effect_camera_keyframes_21_t cleanup;                   /* 0x16b6 */
} effect_misc_data_t;
typedef char
    effect_misc_data_selector_offset_must_be_0x806[((unsigned long)&((effect_misc_data_t*)0)->selector == 0x806) ? 1
                                                                                                                 : -1];
typedef char
    effect_misc_data_position_offset_must_be_0x6d4[((unsigned long)&((effect_misc_data_t*)0)->position == 0x6d4) ? 1
                                                                                                                 : -1];
typedef char
    effect_misc_data_main_offset_must_be_0x14e6[((unsigned long)&((effect_misc_data_t*)0)->main == 0x14e6) ? 1 : -1];
typedef char effect_misc_data_size_must_be_0x1886[(sizeof(effect_misc_data_t) == 0x1886) ? 1 : -1];

/* The four GTE input corners at 0x801bbf40; only the leading x/y word of each
 * is written here, straight from the sprite frame record. */
typedef union battle_effect_corner {
    SVECTOR vector;
    s32 packed;
} battle_effect_corner_t;

/* Variable-offset sprite table consumed by 0x801a5798. */
typedef struct battle_effect_sprite_table_header {
    s16 offsets_offset;
} battle_effect_sprite_table_header_t;

typedef struct battle_effect_sprite_frame {
    u16 flags; /* 0x00; 0x200 selects the semi-transparent code, 0xe0 the texture-page mode bits */
    u8 _unused_02[6];
    u8 u;           /* 0x08 */
    u8 v;           /* 0x09 */
    u8 width;       /* 0x0a */
    u8 height;      /* 0x0b */
    s32 corners[4]; /* 0x0c; packed x/y pairs for the four quad corners */
} battle_effect_sprite_frame_t;

/* The POLY_FT4 the slot double-buffers. include/psx/gpu.h spells the tag as one
 * word and the colour as four bytes; this routine writes the tag's length byte
 * and copies the colour as a word, so both are named here. */
typedef struct battle_effect_quad_prim {
    u8 _unused_00[3]; /* 0x00; tag address */
    u8 length;        /* 0x03; tag word count */
    union {
        u32 packed;
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 code;
        } field;
    } color; /* 0x04 */
    s32 xy0; /* 0x08 */
    u8 u0;
    u8 v0;
    u16 clut; /* 0x0e */
    s32 xy1;  /* 0x10 */
    u8 u1;
    u8 v1;
    u16 tpage; /* 0x16 */
    s32 xy2;   /* 0x18 */
    u8 u2;
    u8 v2;
    u16 _padding_1e; /* aligns xy3 */
    s32 xy3;         /* 0x20 */
    u8 u3;
    u8 v3;
    u16 _padding_26; /* tail padding to 4-byte alignment */
} battle_effect_quad_prim_t;

/* The 0xcc battle_effect_slot_t, with the fields this sprite
 * path uses named. The colour word's top byte is the sprite frame index: the
 * quad's code byte overwrites it immediately after the word copy. */
typedef struct battle_effect_sprite_slot {
    u8 _unused_00[4];
    DR_MODE modes[2][2];                /* 0x04; two draw modes per buffer */
    battle_effect_quad_prim_t quads[2]; /* 0x34 */
    union {
        u32 packed;
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 frame;
        } field;
    } color;  /* 0x84 */
    u16 clut; /* 0x88 */
    u8 _unused_8a[4];
    s16 angle; /* 0x8e */
    u8 _unused_90[0xc];
    s32 x; /* 0x9c */
    s32 y; /* 0xa0 */
    s32 z; /* 0xa4 */
    u8 _unused_a8[0x24];
} battle_effect_sprite_slot_t;
typedef char battle_effect_sprite_slot_size_must_be_0xcc[sizeof(battle_effect_sprite_slot_t) == 0xcc ? 1 : -1];

typedef char
    battle_effect_quad_prim_size_must_match_ft4[sizeof(battle_effect_quad_prim_t) == sizeof(POLY_FT4) ? 1 : -1];

/* Section table at the start of an effect file's header section: byte offsets
 * from the table itself, one per battle_effect_*_address/_pointer getter. */
typedef struct effect_file_header {
    s32 frame_data;      /* 0x00: the getter skips the section's leading word */
    s32 parameter_sets;  /* 0x04: likewise */
    s32 motion_header;   /* 0x08: effect script section */
    s32 particle_system; /* 0x0c: emitter motion and control */
    s32 animation_curve; /* 0x10 */
    s32 nibble_table;    /* 0x14 */
    s32 flags_section;   /* 0x18 */
    s32 misc_data;       /* 0x1c: timing, palette and camera tracks */
    s32 sound_section;   /* 0x20: 0 when the effect has no sound */
    s32 palette;         /* 0x24 */
} effect_file_header_t;
typedef char effect_file_header_size_must_be_0x28[sizeof(effect_file_header_t) == 0x28 ? 1 : -1];

/* Palette-section offsets confirmed by Effect Palette Setup. */
enum {
    EFFECT_FILE_PALETTE_SECOND_BLOCK_OFFSET = 0x200,
    EFFECT_FILE_PALETTE_SIZE_OFFSET = 0x400,
    EFFECT_FILE_PALETTE_IMAGE_OFFSET = 0x404,
};

enum { EFFECT_SCRIPT_OPCODE_MASK = 0x1ff };

typedef enum effect_script_result {
    EFFECT_SCRIPT_RESULT_YIELD = 0,
    EFFECT_SCRIPT_RESULT_CONTINUE = 1,
    EFFECT_SCRIPT_RESULT_ABORT = 2,
} effect_script_result_e;

/* The particle updater reserves -1 for animation-driven lifetime. */
enum { EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN = -1 };

typedef enum battle_effect_sound_mode {
    EFFECT_SOUND_MODE_FIRST = 0,
    EFFECT_SOUND_MODE_ALTERNATE = 1,
    EFFECT_SOUND_MODE_FIRST_THEN_SECOND = 2,
    EFFECT_SOUND_MODE_FIRST_THEN_ALTERNATE = 3,
    EFFECT_SOUND_MODE_CYCLE = 4,
} battle_effect_sound_mode_e;

typedef struct effect_sound_channel {
    u8 mode;         /* 0x00; battle_effect_sound_mode_e */
    u8 sound_ids[3]; /* 0x01 */
} effect_sound_channel_t;

/* Effect flags section at header[0x18] of an effect file. The engine loads
 * flags as a word and tests single bits; spawn_delay_override replaces the
 * timeline's spawn_delay only while g_battle_effect_coord_data.math_skill_flag is
 * set. */
typedef struct effect_flags_section {
    u32 flags;                          /* 0x00; EFFECT_FLAG_* bits */
    u8 spawn_delay_override;            /* 0x04 */
    u8 _unused_05[3];                   /* 0x05 */
    effect_sound_channel_t channels[4]; /* 0x08 */
} effect_flags_section_t;
typedef char effect_flags_section_size_must_be_0x18[(sizeof(effect_flags_section_t) == 0x18) ? 1 : -1];

/* effect_flags_section_t::flags bits read by the engine. */
enum {
    EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST = 0x08, /* anchor Y follows the tile height */
    EFFECT_FLAG_AUDIO_FADE = 0x10,            /* fade effect audio at the end */
    EFFECT_FLAG_TIMING_CURVE_PHASE1 = 0x20,   /* timeline-frame time scale curve */
    EFFECT_FLAG_TIMING_CURVE_PHASE2 = 0x40,   /* animate-tick time scale curve */
};

/* Per-handler phase byte shared by every EFFECT overlay state machine. */
typedef enum effect_phase {
    EFFECT_PHASE_IDLE = 0,
    EFFECT_PHASE_INIT = 1,
    EFFECT_PHASE_UPDATE = 2,
    EFFECT_PHASE_DESTROY = 3,
} effect_phase_e;

/* Emitter spawn origin: bits 9-11 of an emitter's placement word (byte 0x03
 * mask 0x0e, also known as emitter_anchor_mode). 0xa00 is the map centre:
 * every handler adds battle_map_store_max_coordinates() * 14 there. */
typedef enum effect_emitter_origin {
    EFFECT_EMITTER_ORIGIN_MASK = 0xe00,
    EFFECT_EMITTER_ORIGIN_WORLD = 0x000,
    EFFECT_EMITTER_ORIGIN_CURSOR_TILE = 0x200, /* centre of g_battle_effect_target_tile */
    EFFECT_EMITTER_ORIGIN_CASTER = 0x400,
    EFFECT_EMITTER_ORIGIN_TARGET = 0x600,
    EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE = 0x800, /* position of the spawning work record */
    EFFECT_EMITTER_ORIGIN_MAP_CENTRE = 0xa00,
    EFFECT_EMITTER_ORIGIN_TRACKED_ENTITY = 0xc00, /* target plus its unit coordinates */
} effect_emitter_origin_e;

/* Point particles home toward: bits 5-7 of the placement word (byte 0x02 mask
 * 0xe0, also known as target_anchor_mode). 0x00 and 0x20 both leave the
 * interpolated offset unchanged; 0x40 is the map centre. */
typedef enum effect_emitter_homing {
    EFFECT_EMITTER_HOMING_MASK = 0xe0,
    EFFECT_EMITTER_HOMING_OFFSET = 0x00,
    EFFECT_EMITTER_HOMING_OFFSET_ALT = 0x20,
    EFFECT_EMITTER_HOMING_MAP_CENTRE = 0x40,
    EFFECT_EMITTER_HOMING_CASTER = 0x60,
    EFFECT_EMITTER_HOMING_TARGET = 0x80,
    EFFECT_EMITTER_HOMING_CURSOR_TILE = 0xa0,
} effect_emitter_homing_e;

/* Initial particle velocity mode: the velocity_inward bit (byte 0x06 mask
 * 0x10) and align_to_unit_facing bit (byte 0x07 mask 0x04) of the emitter's
 * child/velocity word. OUTWARD scatters around the base angles, INWARD aims at
 * the spawn point, CASTER_FACING rotates OUTWARD by the caster's facing, and
 * TOWARD_TARGET (battle_effect_spawn_particle_motion) rotates it along the
 * caster-to-target direction. */
typedef enum effect_emitter_velocity {
    EFFECT_EMITTER_VELOCITY_MASK = 0x410,
    EFFECT_EMITTER_VELOCITY_OUTWARD = 0x000,
    EFFECT_EMITTER_VELOCITY_INWARD = 0x010,
    EFFECT_EMITTER_VELOCITY_TOWARD_TARGET = 0x400,
    EFFECT_EMITTER_VELOCITY_CASTER_FACING = 0x410,
} effect_emitter_velocity_e;

/* Ordering-table depth clamp shared by the EFFECT renderers: a depth below MIN
 * becomes MIN and one at or beyond LIMIT (the OT slot count) becomes MAX. */
enum {
    EFFECT_OT_DEPTH_MIN = 4,
    EFFECT_OT_DEPTH_MAX = 0x17f,
    EFFECT_OT_DEPTH_LIMIT = 0x180,
};

enum {
    EFFECT_GEOMETRY_FLAG_COLOUR_CURVES = 0x40,
    EFFECT_TEXTURE_PAGE_FLAG_CLEAR_ABE = 0x04,
    EFFECT_WORK_FLAG_COLOUR_CURVES = 0x40,
    EFFECT_WORK_KIND_FLAG_ORIENT_TO_VELOCITY = 0x0002,
    EFFECT_WORK_KIND_MASK = 0xf000,
    EFFECT_WORK_KIND_LIST_NODE_0 = 0x0000,
    EFFECT_WORK_KIND_LIST_NODE_2 = 0x1000,
    EFFECT_WORK_KIND_LIST_NODE_4 = 0x2000,
    EFFECT_WORK_KIND_LIST_NODE_6 = 0x3000,
    EFFECT_WORK_KIND_HEAP_BLOCK = 0x4000,
};

/* Effect timeline keyframe action word (Effect File Timeline, section 11). */
enum {
    EFFECT_KEYFRAME_ACTION_CALLBACK_MASK = 0x0007,
    EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY = 0x0008,
    EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY = 0x0010,
    EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY = 0x0020,
    EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION = 0x0040,
    EFFECT_KEYFRAME_ACTION_ANIMATION_MASK = 0xff00,
    EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT = 8,
    EFFECT_ANIMATION_CURVE_FRAME_BYTES = 0xa0,
};

/* Particle behavior flags from the shared 0x58-byte work record. */
enum {
    EFFECT_WORK_BEHAVIOR_DEATH_CHILD_MASK = 0x0003,
    EFFECT_WORK_BEHAVIOR_DEATH_CHILD_MODE_1 = 0x0001,
    EFFECT_WORK_BEHAVIOR_DEATH_CHILD_MODE_2 = 0x0002,
    EFFECT_WORK_BEHAVIOR_MID_LIFE_CHILD_MASK = 0x000c,
    EFFECT_WORK_BEHAVIOR_MID_LIFE_CHILD_MODE_1 = 0x0004,
    EFFECT_WORK_BEHAVIOR_MID_LIFE_CHILD_MODE_2 = 0x0008,
    EFFECT_WORK_BEHAVIOR_HOMING_THRESHOLD_MASK = 0x0300,
    EFFECT_WORK_BEHAVIOR_HOMING_THRESHOLD_SHIFT = 4,
};

typedef struct battle_effect_rotation_vector {
    u16 x;
    u16 y;
    u16 z;
    u16 _unused_06;
} battle_effect_rotation_vector_t;

/* Color and count header followed by one pointer for each sprite part. */
typedef struct battle_effect_sprite_block {
    union {
        u32 packed;
        struct {
            u8 red;
            u8 green;
            u8 blue;
            u8 sprite_count;
        } field;
    } color;
    u8 _unused_04[4];
    struct battle_effect_sprite_part* frames[1];
} battle_effect_sprite_block_t;

/* Temporary effect-script workspace cleared by opcode 0x2A. */
typedef struct battle_effect_temp_data {
    u8 _unused_00[0x26];
    s16 target_index; /* 0x26 */
    s16 frame;        /* 0x28 */
    s16 values_2a[13];
    s16 values_44[13];
    s16 values_5e[13];
} battle_effect_temp_data_t;
typedef char battle_temp_effect_data_size_must_be_0x78[sizeof(battle_effect_temp_data_t) == 0x78 ? 1 : -1];

/* 0x24-byte node of the active-effect list at 0x801c00a4. Links are node
 * indexes into that array; node 0 is the sentinel.
 * g_battle_effect_active_list_node_head is the list head and g_battle_effect_free_list_node_head the
 * free-list head. The record is also known as ParticleAnimState; its field at
 * 0x1c is a byte offset into the sequence, not a frame counter. */
typedef struct effect_list_node {
    u16 prev;                  /* 0x00 */
    u16 next;                  /* 0x02 */
    u16 index;                 /* 0x04: this node's own array index */
    s16 kind;                  /* 0x06: 0, 2, 4 or 6 from battle_effect_alloc_work; bit 0 marks a new frame */
    s16 sprite_offset_x;       /* 0x08: added to x when drawn */
    s16 sprite_offset_y;       /* 0x0a: added to y when drawn */
    s16 x;                     /* 0x0c: from the owning work record's position */
    s16 y;                     /* 0x0e */
    s16 z;                     /* 0x10 */
    s16 screen_rotation_angle; /* 0x12: battle_effect_update_node_from_work stores the velocity heading here */
    u8 depth_mode;             /* 0x14: third byte of the frame opcode; OT depth adjustment */
    u8 _padding_15;            /* aligns frame_timer */
    s16 frame_timer;           /* 0x16: counts down by 2; the next opcode runs at <= 0 */
    u8* sequence_data;         /* 0x18: base of the u8 animation sequence */
    s16 sequence_offset;       /* 0x1c: byte offset of the next sequence opcode */
    s8 frame_group_index;      /* 0x1e: indexes g_battle_effect_frame_data */
    s8 sprite_frame_index;     /* 0x1f: frame id from the sequence opcode */
    battle_effect_sprite_block_t* sprite_block; /* 0x20 */
} effect_list_node_t;
typedef char assert_effect_list_node_size[sizeof(effect_list_node_t) == 0x24 ? 1 : -1];

/* 0x58-byte effect work record. battle_effect_init_record_chain allocates 256
 * of them as one doubly linked free chain; while attached to an effect state
 * record it sits on that record's work list (effect_record_t.work_head). */
typedef struct effect_work_record {
    struct effect_work_record* prev; /* 0x00 */
    struct effect_work_record* next; /* 0x04 */
    u8 _unused_08[0x04];
    s32 position[3]; /* 0x0c: x/y/z, 20.12 fixed; battle_effect_update_node_from_work copies >> 12 to the node */
    s32 velocity[3]; /* 0x18: x/y/z; battle_effect_update_node_from_work orients the node along it */
    s32 wave_y[3];   /* 0x24: randomized vertical-wave components from the emitter */
    s32 wave_x[3];   /* 0x30: randomized horizontal-wave components from the emitter */
    s16 anchor_x;    /* 0x3c: compared against position x >> 12 */
    s16 anchor_y;    /* 0x3e: compared against position y >> 12 */
    s16 anchor_z;    /* 0x40: compared against position z >> 12 */
    s16 lifetime;    /* 0x42: decremented by the particle update */
    u8 _unused_44[0x02];
    u8 colour_curves[3]; /* 0x46: r/g/b rows of g_effect_palette_table */
    u8 _unused_49[0x03];
    u16 kind_flags;             /* 0x4c: bits 12-15 select how data is allocated/released; bit 1 orients to velocity */
    u16 flags;                  /* 0x4e: bit 6 = per-channel colour curves in use */
    s16 palette_frame;          /* 0x50: column of the colour-curve rows */
    u8 child_emitter_index;     /* 0x52: emitter index for mid/death children */
    u8 child_emitter_index_alt; /* 0x53: alternate emitter index for children */
    struct effect_list_node* data; /* 0x54: effect_list_node_t for kinds 0x0000-0x3000, heap block for 0x4000 */
} effect_work_record_t;
typedef char assert_effect_work_record_size[sizeof(effect_work_record_t) == 0x58 ? 1 : -1];

/* A particle-physics view of the 0x58-byte work record. It is not
 * interchangeable with effect_work_record_t: the target's native work-list
 * handlers use several of these offsets for different state (for example,
 * 0x24/0x30 are wave or force-step vectors depending on the handler).
 * battle_effect_spawn_emitter_particles initialises battle_effect_alloc_work
 * records through this view. */
typedef struct effect_particle_physics_view {
    u8 _unused_00[0x04];
    struct effect_particle_physics_view* next; /* 0x04 */
    s16 inertia;                               /* 0x08 */
    s16 weight;                                /* 0x0a */
    s32 position[3];                           /* 0x0c */
    s32 velocity[3];                           /* 0x18 */
    s32 acceleration[3];                       /* 0x24 */
    s32 drag[3];                               /* 0x30 */
    s16 target[3];                             /* 0x3c */
    s16 lifetime;                              /* 0x42 */
    u8 _unused_44;
    u8 homing_curve_index;     /* 0x45 */
    u8 colour_curve_index[3];  /* 0x46 */
    u8 _padding_49;            /* aligns homing_strength */
    s16 homing_strength;       /* 0x4a */
    u16 kind_flags;            /* 0x4c: the emitter's work kind flags (battle_effect_spawn_emitter_particles) */
    u16 child_spawn_flags;     /* 0x4e */
    s16 animation_frame;       /* 0x50 */
    u8 child_emitter_on_death; /* 0x52 */
    u8 child_emitter_mid_life; /* 0x53 */
    void* animation_state;     /* 0x54 */
} effect_particle_physics_view_t;
typedef char assert_effect_particle_physics_view_size[sizeof(effect_particle_physics_view_t) == 0x58 ? 1 : -1];

typedef struct effect_record {
    u16 next_index;     /* 0x00: next record in the current intrusive list */
    s16 record_index;   /* 0x02: initialized to this record's array index */
    s16 parent_index;   /* 0x04: spawning record (g_battle_effect_current_record_index) for script opcode 02 */
    s16 pc;             /* 0x06: byte offset of the current instruction in script */
    u8* script;         /* 0x08 */
    s16 phase_ids[4];   /* 0x0c: child record indexes stored by script opcode 02 */
    s16 counters[4];    /* 0x14: opcodes 20..23 select one by the argument's top two bits */
    u16 work_count;     /* 0x1c: entries on work_head */
    u16 flags;          /* 0x1e: bit 0 marks an allocated record with a work list */
    s16 emitter_index;  /* 0x20: passed by script opcode 26; reset to -1 */
    u8 phase[4];        /* 0x22: per-handler phase, indexed by the handler's byte_offset */
    u8 target_index;    /* 0x26: current target, resolved to an anchor position by the emitter and native handlers */
    u8 _padding_27;     /* aligns timeline_frame */
    s16 timeline_frame; /* 0x28: frame counter of battle_effect_code_script_29_step_parent_timeline; tested by opcodes
                           1d/1e */
    u8 _unused_2a[0xa6];
    effect_work_record_t* work_head; /* 0xd0 */
    s32 values_d4[4];                /* 0xd4: cleared at allocation */
    void* work_slots[5];             /* 0xe4: indexed by the same byte_offset */
} effect_record_t;
typedef char assert_effect_record_size[sizeof(effect_record_t) == 0xf8 ? 1 : -1];

/* View of effect_record_t with the s16 at 0x26, read with lh as a
 * coordinate-target index. The access must be a member of an array element
 * (folded %lo(sym + 0x26) on the shared record_index * 0xf8 register); a
 * pointer cast CSEs into `record` instead. g_effect_state_records_view is
 * g_effect_state_records through this view. */
typedef struct effect_record_target_view {
    u8 _unused_00[0x26];
    s16 target_index; /* 0x26 */
    u8 _unused_28[0xd0];
} effect_record_target_view_t;
typedef char assert_effect_record_target_view_size[sizeof(effect_record_target_view_t) == 0xf8 ? 1 : -1];

/* One playback step of the animation script referenced by an effect slot:
 * a hold count and the frame id written to the slot. */
typedef struct battle_effect_anim_step {
    u8 hold;  /* 0x00 */
    u8 frame; /* 0x01 */
} battle_effect_anim_step_t;

typedef struct battle_effect_anim_script {
    s16 count;                         /* 0x00 */
    battle_effect_anim_step_t step[1]; /* 0x02 */
} battle_effect_anim_script_t;

/* 0xcc-byte effect slots at 0x801b9278, chained through the leading next/prev
 * byte pair; slot 0 is the null sentinel. g_battle_effect_active_slot_head heads the active chain
 * and g_battle_effect_slot_free_head the free chain. 0x84..0x87 has the battle_effect_sprite_block_t
 * layout (colour plus the frame byte); battle_effect_init_trap_animation resets
 * the colour and writes the halfwords at 0x88 and 0x8e. */
/* Per-slot motion state at battle_effect_slot_t +0x98, also embedded in the
 * particle list nodes battle_effect_integrate_particle_motion integrates. Velocity decays by
 * g_battle_effect_inertia_threshold / mass each step, gains force << 12 /
 * mass, and gains on_hit_effects_data scaled by on_hit_weight (4.12 fixed
 * point). */
typedef struct battle_effect_motion {
    s16 mass;          /* 0x00 */
    s16 on_hit_weight; /* 0x02 */
    s32 position[3];   /* 0x04 */
    s32 velocity[3];   /* 0x10 */
    s32 force[3];      /* 0x1c */
    s32 force_step[3]; /* 0x28 */
} battle_effect_motion_t;
typedef char battle_effect_motion_size_must_be_0x34[(sizeof(battle_effect_motion_t) == 0x34) ? 1 : -1];

typedef struct battle_effect_slot {
    u8 next;                             /* 0x00 */
    u8 prev;                             /* 0x01 */
    u8 _padding_02[2];                   /* aligns modes */
    DR_MODE modes[2][2];                 /* 0x04; two draw modes per buffer */
    POLY_FT4 quads[2];                   /* 0x34; one quad per buffer */
    u8 red;                              /* 0x84 */
    u8 green;                            /* 0x85 */
    u8 blue;                             /* 0x86 */
    u8 frame;                            /* 0x87 */
    u16 clut;                            /* 0x88 */
    s16 step_index;                      /* 0x8a */
    s16 hold;                            /* 0x8c */
    s16 angle;                           /* 0x8e; billboard quad rotation */
    battle_effect_anim_script_t* script; /* 0x90 */
    s16 life;                            /* 0x94: negative runs until the script wraps */
    u8 _padding_96[0x02];                /* aligns motion */
    battle_effect_motion_t motion;       /* 0x98 */
} battle_effect_slot_t;
typedef char assert_battle_effect_slot_size[sizeof(battle_effect_slot_t) == 0xcc ? 1 : -1];

/* Entry passed to battle_effect_get_timing_curve_value (0x801a1200) and
 * battle_effect_get_animation_timing_curve_value (0x801a1244). The owning
 * record is unidentified; only the halfword at 0x28 is proven. */
typedef struct battle_effect_timing_entry {
    u8 _unused_00[0x28];
    u16 curve_index; /* 0x28: nibble index into g_battle_effect_nibble_table; bit 0 selects the high nibble */
} battle_effect_timing_entry_t;

/* The particle emitter's view of effect_geometry_entry_t 0x14-0x4b. The
 * spread is the ellipsoid or box half-extent around the spawn position and the
 * velocity spread randomizes the base angles. Min, max pairs such as inertia
 * give the range of the random value stored in each new work record. */
typedef struct effect_emitter_geometry_view {
    s16 spawn_position_start[3];      /* 0x14 */
    s16 spawn_position_end[3];        /* 0x1a */
    s16 spawn_spread_start[3];        /* 0x20 */
    s16 spawn_spread_end[3];          /* 0x26 */
    s16 velocity_base_angle_start[3]; /* 0x2c */
    s16 velocity_base_angle_end[3];   /* 0x32 */
    s16 velocity_spread_start[3];     /* 0x38 */
    s16 velocity_spread_end[3];       /* 0x3e */
    s16 inertia_start[2];             /* 0x44: min, max; stored at the work record's +0x08 */
    s16 inertia_end[2];               /* 0x48 */
} effect_emitter_geometry_view_t;

typedef struct effect_textured_quad_geometry_view {
    u16 texture_u; /* 0x14 */
    u16 texture_v; /* 0x16 */
    u8 _unused_04[0x16];
    s16 scroll_angle_start; /* 0x2e */
    u8 _unused_1c[0x04];
    s16 scroll_angle_end; /* 0x34 */
    u8 _unused_22[0x02];
    s16 wave_phase_speed_start[2]; /* 0x38 */
    u8 _unused_28[0x02];
    s16 wave_phase_speed_end[2]; /* 0x3e */
    u8 _unused_2e[0x0a];
} effect_textured_quad_geometry_view_t;

typedef union effect_geometry_motion_view {
    effect_emitter_geometry_view_t emitter;
    effect_textured_quad_geometry_view_t textured_quad;
} effect_geometry_motion_view_t;
typedef char assert_effect_geometry_motion_view_size[sizeof(effect_geometry_motion_view_t) == 0x38 ? 1 : -1];

/* One 0xc4-byte emitter record of the effect file's particle-system section.
 * The BATTLE particle emitter at 0x801a60ac reads it with the meanings named
 * here; native EFFECT renderers reuse the same slots and curve bindings for
 * their own parameters (the motion views, effect_polar_mesh_geometry_view_t),
 * and the fields they read through this type keep the renderer's name.
 *
 * work_kind_flags: bit 1 orients the particle to its velocity, bits 5-7 pick
 * the homing target anchor, bit 8 selects box rather than ellipsoid spread,
 * bits 9-11 pick the spawn anchor (0x200 tile target, 0x400 caster, 0x600
 * target_index of the effect record, 0x800 parent particle, 0xa00 map centre)
 * and bits 12-15 the work-record allocation kind. flags bits 0-1 and 2-3
 * enable death and mid-life children, and flags & 0x410 selects outward,
 * inward or facing-rotated velocity.
 *
 * Each curve nibble is an index into g_effect_palette_table (0 = none,
 * otherwise row + 1) whose factor lerps one start/end block:
 *   curve_indices[0]: 0 spawn position, 1 spawn spread, 2 velocity base angle,
 *     3 velocity spread, 4 inertia, 5 0x4c/0x50 (lerped and discarded by the
 *     emitter), 6 weight, 7 radial speed;
 *   curve_indices[1]: 0 acceleration, 1 drag, 2 lifetime, 3 target offset,
 *     4 0xa8/0xac (unused by the emitter), 5 particle count, 6 spawn
 *     interval, bits 28-29 homing strength; bits 30-31 are copied to the work
 *     record's +0x45 homing-blend curve. */
typedef struct effect_geometry_entry {
    u8 _unused_00;
    u8 animation_index;       /* 0x01: passed to battle_effect_init_parameter_set as the sequence */
    u16 work_kind_flags;      /* 0x02: copied to the work record's kind_flags */
    u8 animation_frame_group; /* 0x04: passed to battle_effect_init_parameter_set, stored doubled at node +0x1e */
    u8 _padding_05;           /* aligns flags */
    u16 flags;                /* 0x06: copied to the work record's flags; bit 6 = per-channel colour curves */
    u32 curve_indices[2];     /* 0x08 */
    u32 colour_curves;        /* 0x10: nibbles 0-2 = r/g/b */
    effect_geometry_motion_view_t motion; /* 0x14: effect-kind-specific fields */
    u16 texture_page_flags;               /* 0x4c: native renderers; bits 0-1 tpage x, bit 2 clears ABE */
    s16 brightness_row;                   /* 0x4e: native renderers */
    /* 0x50/0x52: the emitter lerps 0x4c->0x50 and 0x4e->0x52 and discards the results. */
    s16 lerp_end_4c;
    s16 lerp_end_4e;
    s16 ot_depth;               /* 0x54: native renderers; the emitter's minimum start weight */
    s16 weight_max_start;       /* 0x56: weight pairs are min, max, stored at the work record's +0x0a */
    s16 weight_end[2];          /* 0x58: min, max */
    s16 scroll_speed_start;     /* 0x5c: textured-quad grids; the emitter's minimum start radial speed */
    s16 radial_speed_max_start; /* 0x5e */
    s16 scroll_speed_end;       /* 0x60 */
    s16 radial_speed_max_end;   /* 0x62 */
    s16 wave_y_start[6];        /* 0x64: emitter acceleration x/y/z min, max pairs (work +0x24) */
    s16 wave_y_end[6];          /* 0x70 */
    s16 wave_x_start[6];        /* 0x7c: emitter drag x/y/z min, max pairs (work +0x30) */
    s16 wave_x_end[6];          /* 0x88 */
    s16 lifetime_start[2];      /* 0x94: min, max, stored as the work record's lifetime */
    s16 lifetime_end[2];        /* 0x98 */
    s16 target_offset_start[3]; /* 0x9c: added to the target anchor chosen by work_kind_flags bits 5-7 */
    s16 target_offset_end[3];   /* 0xa2 */
    s16 texture_width;          /* 0xa8: native renderers */
    s16 texture_height;         /* 0xaa */
    u8 _unused_ac[0x04];
    s16 particle_count_start;     /* 0xb0 */
    s16 particle_count_end;       /* 0xb2 */
    s16 spawn_interval_start;     /* 0xb4: frames between spawns */
    s16 spawn_interval_end;       /* 0xb6 */
    s16 homing_strength_start[2]; /* 0xb8: min, max, stored at the work record's +0x4a */
    s16 homing_strength_end[2];   /* 0xbc */
    u8 child_emitter_on_death;    /* 0xc0: used when flags bits 0-1 are set */
    u8 child_emitter_mid_life;    /* 0xc1: used when flags bits 2-3 are set */
    u8 _padding_c2[0x02];         /* tail padding to 4-byte alignment */
} effect_geometry_entry_t;
typedef char assert_effect_geometry_entry_size[sizeof(effect_geometry_entry_t) == 0xc4 ? 1 : -1];

/* Particle-system section addressed by effect-file header word 0x0c. */
typedef struct effect_geometry_table {
    u16 _unused_00;        /* 2, or 1 in seven shipped files */
    u16 entry_count;       /* 0x02 */
    s32 gravity[3];        /* 0x04: copied to on_hit_effects_data by effect script opcode 0x27 */
    s32 inertia_threshold; /* 0x10: passed to battle_effect_set_inertia_threshold by opcode 0x27 */
    effect_geometry_entry_t entries[1];
} effect_geometry_table_t;

/* Animation-curve row: one interpolation factor per frame. The section begins
 * with a u32 curve count and each curve is 0xa0 bytes, so a row's leading
 * four bytes are that count (row 0) or the last four frames of the previous
 * row, and factor[] indexes past 0x9b continue into the next row. */
typedef struct effect_palette_entry {
    u8 _unused_00[4];
    u8 factor[0x9c];
} effect_palette_entry_t;
typedef char assert_effect_palette_entry_size[sizeof(effect_palette_entry_t) == 0xa0 ? 1 : -1];

/* Disc extent of a BATTLE resource file. */
typedef struct battle_file_extent {
    s32 sector;
    s32 size;
} battle_file_extent_t;

extern battle_effect_secondary_animation_t g_battle_effect_secondary_animations[];
extern battle_effect_secondary_data_t g_battle_effect_secondary_data[];
extern u8 g_effect_load_state;
extern u8 g_battle_effect_weapon_type_extra_flags;
extern s32 g_battle_effect_camera_rotation_mode;
extern s32 g_battle_effect_camera_position_mode;
extern s32 g_battle_effect_camera_zoom_mode;
extern u8 g_battle_effect_trap_frame_data[];
extern u8 g_battle_effect_ninja_ball_masks[];
extern s32 g_battle_effect_gravity_scale;
extern MATRIX g_battle_effect_particle_heading_matrix;
extern MATRIX g_battle_effect_emitter_matrix;
extern MATRIX g_battle_effect_projectile_matrix;
extern s32* g_battle_effect_model_header;
extern s32* g_battle_effect_model_vertices;
extern s32* g_battle_effect_model_commands;
extern s32 g_battle_effect_model_command_index;
extern SVECTOR g_battle_effect_camera_rotation_start;
extern SVECTOR g_battle_effect_camera_rotation_target;
extern SVECTOR g_battle_effect_camera_rotation_current;
extern SVECTOR g_battle_effect_camera_rotation_saved;
extern VECTOR g_battle_effect_camera_position_start;
extern VECTOR g_battle_effect_camera_position_target;
extern VECTOR g_battle_effect_camera_position_current;
extern VECTOR g_battle_effect_camera_position_saved;
extern s32 g_battle_effect_camera_zoom_duration;
extern s32 g_battle_effect_camera_zoom_frame;
extern VECTOR g_battle_effect_camera_zoom_start;
extern VECTOR g_battle_effect_camera_zoom_target;
extern VECTOR g_battle_effect_camera_zoom_current;
extern VECTOR g_battle_effect_camera_zoom_saved;
extern s32 g_battle_effect_trajectory_hit_unit_id;
extern void* g_battle_effect_trap_frame_data_ptr;
extern s32 g_battle_effect_callback_slots[];
extern u8 g_battle_effect_palette_target_misc_ids[];
extern SVECTOR g_battle_effect_trajectory_origin;
extern SVECTOR g_battle_effect_trajectory_destination;
extern s32 g_battle_effect_trajectory_step_count;
extern u8* g_battle_effect_palette_ptr;

/* Per-group trap animation records. The byte at +0x00 of each 0x2e-byte record
 * is an index into g_battle_effect_trap_animation_data; the
 * battle_effect_group_t model starts two bytes
 * later, with spawn_start_frame at 0x801b8566, so this table is that same record array
 * addressed from its true base. */
extern u8 g_battle_effect_trap_particle_configs[];
extern s16 g_battle_effect_root_record_index;
extern battle_effect_resource_t* g_battle_effect_model_data_ptrs[];
extern battle_file_extent_t g_battle_effect_disc_entries[];
extern battle_effect_secondary_state_t g_battle_effect_secondary_state;
extern battle_effect_target_t g_battle_effect_target_tile;
extern battle_effect_tile_ref_t g_battle_effect_trajectory_tile;
extern s16 g_battle_effect_ability_ids[];
extern u8* g_battle_effect_animation_curve_data;
extern s16 g_battle_effect_arctan_angle_mod;
extern s32 g_battle_effect_buffer_index;
extern battle_effect_coord_data_t g_battle_effect_coord_data;
extern u16 g_battle_effect_free_slot_head;
extern struct battle_effect_secondary_data* g_battle_effect_current_secondary;
extern u32 g_battle_effect_current_slot_id;
extern effect_file_header_t* g_battle_effect_data_ptrs[];
extern void* g_battle_effect_file_ptrs[];
extern u8* g_battle_effect_frame_data;
extern u8* g_battle_effect_frame_data_start;
extern s32 g_battle_effect_gravity_modifier;
extern battle_effect_group_t g_battle_effect_groups[];
extern s32 g_battle_effect_inertia_threshold;
extern s16 g_battle_effect_load_countdown;
extern effect_misc_data_t* g_battle_effect_misc_data;
extern u8* g_battle_effect_motion_header;
extern u8* g_battle_effect_nibble_table;
extern battle_effect_tick_channels_t* g_battle_effect_timing_channels;
extern u8* g_battle_effect_parameter_sets;
extern u8* g_battle_effect_parameter_sets_start;
extern u32* g_battle_effect_polygon_depth_data;
extern u16 g_battle_effect_active_slot_tail;
extern s32 g_battle_effect_primitive_sizes[];
extern s32 g_battle_effect_saved_inertia_threshold;
extern s32 g_battle_effect_sound_data_base;
extern struct main_sound_resource* g_battle_effect_sound_section_ptr;
extern u8 g_battle_effect_target_id;
extern effect_flags_section_t* g_battle_effect_flags_section;
extern battle_effect_on_hit_vector_t on_hit_effects_data;
extern battle_effect_on_hit_vector_t on_hit_effects_data_second_section;
extern s32 g_battle_effect_arc_trajectory_height;
extern s16 g_battle_effect_arrow_arc_caster_relative_yaw;
extern s32 g_battle_effect_arrow_arc_distance;
extern s32 g_battle_effect_arrow_arc_distance_step;
extern s32 g_battle_effect_arrow_arc_height;
extern s16 g_battle_effect_arrow_arc_launch_angle;
extern s32 g_battle_effect_arrow_arc_length;
extern u16 g_battle_effect_arrow_arc_yaw;
extern s32 g_battle_effect_camera_position_duration;
extern s32 g_battle_effect_camera_position_frame;
extern s32 g_battle_effect_camera_rotation_duration; /* angle track duration */

extern s32
    g_battle_effect_camera_rotation_frame; /* angle track counter; reset by battle_camera_execute_angle_command */

extern DVECTOR g_battle_effect_charge_line_direction_offsets[];
extern u8 g_battle_effect_charge_line_fade_levels[];
extern SVECTOR g_battle_effect_corner_bottom_left;
extern SVECTOR g_battle_effect_corner_bottom_right;
extern SVECTOR g_battle_effect_corner_top_left;
extern SVECTOR g_battle_effect_corner_top_right;
extern u8 g_battle_effect_ending_secondary_function_id; /* function byte to store; 5 = nothing */
extern s16 (*g_battle_effect_instructions[])(effect_record_t*);
extern MATRIX g_battle_effect_matrix; /* 0x801bc09c */
extern u32* g_battle_effect_otag;

/* Work-kind handlers, indexed by kind_flags >> 12. The int return type is
 * load-bearing even though the result is ignored: the value-returning call is
 * a second set of $2, so sched1 no longer boosts the final `$2 = 1` to the end
 * of its block, and the pc update is allocated $3 as in the target. */
extern s32 (*g_battle_effect_particle_render_handlers[])(effect_record_t* record, effect_work_record_t* work);
extern VECTOR g_battle_effect_projectile_delta;
extern s32 g_battle_effect_projectile_length;
extern s32 g_battle_effect_projectile_progress;
extern s32 g_battle_effect_projectile_speed;
extern s16 g_battle_effect_projectile_spin_passes;
extern s16 g_battle_effect_projectile_step_count;

/* Six RGB byte triples, packed into both CLUT rows as 15-bit colours. */
extern u8 g_battle_effect_reflect_palette_colors[];

/* Secondary-animation id per ability, indexed by the low halfword of the
 * caller's packed argument. */
extern u8 g_battle_effect_secondary_by_charge_animation[];
extern u8 g_battle_effect_sound_call_counts[]; /* per-channel call counts */

/* Provisional per-column brightness weights (out of 128). */
extern u8 g_battle_effect_summon_ring_brightness[];
extern battle_effect_target_t g_battle_effect_targets[];
extern SVECTOR g_battle_effect_trajectory_position; /* final projectile position */
extern s32 g_battle_effect_trajectory_source_id;
extern VECTOR g_battle_effect_trajectory_step; /* last step's movement */
extern s32 g_battle_effect_trajectory_tile_flags;

/* Self-relative animation script table: each halfword is a byte offset from
 * the table's own base to a battle_effect_anim_script_t. Declared as bytes
 * because both uses are byte-based -- the stored pointer is the table's own
 * base plus a byte offset -- and the halfword entries are read through a cast
 * at the use site. Declaring it `u16[]` and casting the symbol to `u8*` for
 * the address instead reserves an unused 8-byte frame in
 * battle_effect_init_trap_animation. */
extern u8 g_battle_effect_trap_animation_data[];
extern u8 g_battle_effect_trap_colors[][3];

/* Provisional: per-parameter animation-id table read with the secondary
 * effect's parameter halfword. */
extern u8 g_battle_effect_zodiac_poof_animations[];

/* Unsigned halfword indexes: BATTLE list traversal loads these with lhu. */
extern u16 g_battle_effect_allocated_record_head;
extern u16 g_battle_effect_available_record_head;
extern s32 g_battle_effect_frame_parity;
extern s32 g_battle_effect_frame_start_prim_buffer_offset;
extern s32 g_battle_effect_phase;
extern u8* g_battle_effect_prim_buffer;
extern volatile s32 g_battle_effect_prim_buffer_offset;
extern battle_effect_slot_t g_battle_effect_slots[];
extern s32 g_battle_effect_sprite_count;
extern s32 g_battle_effect_sprite_count_peak;
extern effect_geometry_table_t* g_effect_geometry_table;
extern effect_palette_entry_t* g_effect_palette_table;
extern u8* g_effect_particle_system_data;
extern effect_record_t g_effect_state_records[];
extern effect_record_target_view_t g_effect_state_records_view[];
extern s16 g_battle_effect_current_record_index;
extern s32 g_battle_effect_work_record_peak;
extern effect_work_record_t* g_battle_effect_free_work_record_head;
extern u8 g_battle_effect_active_slot_head;
extern s32 g_battle_effect_work_record_count;

/* Effect work pool: g_battle_effect_work_records is the 256-record allocation, g_battle_effect_free_work_record_head
 * the free-chain head, g_battle_effect_work_record_count the in-use count and g_battle_effect_work_record_peak its
 * peak. */
extern effect_work_record_t* g_battle_effect_work_records;
extern u8 g_battle_effect_slot_free_head;
extern u16 g_battle_effect_free_list_node_head;
extern effect_list_node_t g_battle_effect_list_nodes[];
extern u16 g_battle_effect_active_list_node_head;
void battle_effect_set_ability_animation(u32 packed, s32 ability, battle_effect_secondary_init_t* source);
s32 battle_effect_init_fall_dust_secondary(battle_effect_secondary_init_t* source, VECTOR* position);
s32 battle_effect_is_item_ability(s32 ability_id);
void battle_effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);
s32 battle_effect_check_direct_trajectory_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id);
void battle_effect_store_targets_and_display_types(s32 ability_id, battle_unit_misc_data_t* unit);
void battle_effect_free_state_record(s16);

/* The last argument is an optional transform: the EFFECT overlay's particle
 * handlers pass a 0x58-byte record built on their stack, the battle script
 * opcode passes none. The emitter index arrives as a full word. */
void battle_effect_spawn_emitter_particles(
    s16 effect_index, s32 frame, s32 emitter_index, effect_work_record_t* transform);

void battle_effect_build_secondary_init_from_action(
    battle_unit_misc_data_t* unit, battle_effect_secondary_init_t* init);

void battle_effect_call_build_secondary_init(battle_unit_misc_data_t* unit);

void battle_effect_init_altima_teleport_data(
    battle_unit_misc_data_t* unit, battle_effect_secondary_init_t* teleport_data);

s32 battle_effect_init_ninja_ball_secondary(s32 elements, battle_effect_secondary_init_t* source);
void battle_effect_set_evade_type_data_item_and_throw_stone_hardcoding(battle_unit_misc_data_t* unit);
void battle_effect_set_secondary_death_smoke(battle_unit_misc_data_t* unit);
void battle_effect_set_secondary_venom_trap(battle_unit_misc_data_t* unit);
void battle_effect_set_secondary_zodiac_poof(battle_unit_misc_data_t* unit);
s32 battle_effect_start_altima_teleport_arrival(battle_unit_misc_data_t* unit);
s32 battle_effect_start_altima_teleport_departure(battle_unit_misc_data_t* unit);
void battle_effect_start_altima_teleport_fade_out(battle_unit_misc_data_t* unit);
void battle_effect_start_altima_teleport_white_flash(battle_unit_misc_data_t* unit);
s32 battle_effect_init_resource_sections(battle_effect_resource_t* resource);
s32 battle_effect_spawn_particle_motion(battle_effect_emitter_values_t* values, battle_effect_motion_t* motion);
void battle_effect_add_random_vector_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);

void battle_effect_add_random_vector_offsets_and_store_q12(
    const VECTOR* source, const VECTOR* ranges, VECTOR* destination);

void battle_effect_add_vectors(const s32* first, const s32* second, s32* out);

void battle_effect_advance_map_background_track(
    battle_effect_background_track_t* track, s16* keyframe, s16* remaining_frames);

void battle_effect_build_secondary_init(
    struct battle_unit_misc_data* unit, struct battle_effect_secondary_init* effect);

s32 battle_effect_calculate_arc_height_term(s32 distance);
s32 battle_effect_calculate_arcing_trajectory(battle_arc_query_t* query, s32* arc_a, s32* arc_b);
void battle_effect_call_play(void);
void battle_effect_copy_camera_angles_and_screen_coords(void);
void battle_effect_copy_on_hit_data_to_second_section(void);
void battle_effect_copy_second_section_to_on_hit_data(void);
void battle_effect_finalize_secondary(u8 target_id);
s32 battle_effect_find_next_keyframe(s16 frame, u32 mask, s32* out_index, s32* out_frame);
s32 battle_effect_get_bow_shot_angle(void);
s32 battle_effect_allocate_secondary_slot(void);
s32 battle_effect_get_ninja_ball(s32 arg);
s32 battle_effect_get_on_hit_sound_index(s32 channel);
s16 battle_effect_get_random_between_s16(s16 minimum, s16 maximum);
s32 battle_effect_get_secondary_phase(u8 target_id);
void battle_effect_init_flags_section_pointer(s16 effect_id);
void battle_effect_init_camera_and_screen_data(void);
s32 battle_effect_init_projectile_secondary(s32 animation, struct battle_effect_secondary_init* source);
s32 battle_effect_init_secondary(s32 animation_id, s32 sprite_palette, struct battle_effect_secondary_init* source);
void battle_effect_init_system(void);
void battle_effect_interpolate_s16(s16 from, s16 to, s32 t, s16* out);
void battle_effect_interpolate_svector(const s16* from, const s16* to, s32 t, s16* out);
void battle_effect_interpolate_vector_q13(const s32* from, const s32* to, s32 t, s32* out);
s32 battle_effect_lerp_and_store(s32 start, s32 end, s32 factor, s32* out);
void battle_effect_lerp_component_q13(s32 from, s32 to, s32 t, s32* out);
void battle_effect_list_valid_unit_targeting_data(s32 first, s32 second, battle_effect_obstacle_unit_list_t** out);
void battle_effect_mask_vector_low_12_bits(const s16* src, s16* dst);
void battle_effect_load_lba_and_size(s32 effect_id, s32* lba, s32* byte_length);
void battle_effect_play_and_wait_for_animation(void);
void battle_effect_remove_secondary(u8 id);
void battle_effect_replace_flags_0006(struct battle_effect_flag_prefix* object, s32 value);
void battle_effect_reset_render_state(void);
void battle_effect_restore_inertia_threshold(void);
void battle_effect_save_inertia_threshold(void);
void battle_effect_set_evade_type_data_and_weapon_element(struct battle_unit_misc_data* unit, s32 is_bow);
void battle_effect_set_inertia_threshold(s32 threshold);
void battle_effect_set_secondary(struct battle_unit_misc_data* unit, s32 effect);
void battle_effect_set_secondary_fall_dust(struct battle_unit_misc_data* unit);
void battle_effect_set_secondary_level_up(struct battle_unit_misc_data* unit);
void battle_effect_set_secondary_splash(struct battle_unit_misc_data* unit);
void battle_effect_set_secondary_teleport(struct battle_unit_misc_data* unit);
void battle_effect_shift_vector_left_12(const VECTOR* src, VECTOR* dst);
void battle_effect_shift_vector_right_12(const s32* src, s32* dst);
void battle_effect_start_group(s32 group, s32 animation);

void battle_effect_step_emitter_timeline(battle_keyframe_effect_state_t* state, battle_effect_keyframe_table_t* table,
    s16* index, s16* remaining, s16* step);

void battle_effect_step_motion(battle_effect_motion_t* motion);
void battle_effect_store_first_section_of_on_hit_data(battle_effect_on_hit_vector_t* src);

s32 battle_effect_trace_arc_trajectory_path(
    SVECTOR* origin, s32* height, s32* distance, battle_effect_arc_t* arc, void* obstacles);

s32 battle_effect_test_position_for_obstacle(
    battle_effect_obstacle_unit_list_t* list, VECTOR* position, s32* out_unit, battle_effect_tile_ref_t* tile_ref);

s32 battle_effect_try_init_data(void);
void battle_effect_update_active_slots(s32 group);
s32 battle_effect_update_secondary_effects(void);
void battle_effect_add_svector_to_vector(const s16* first, const s32* second, s32* out);
void battle_effect_add_svectors(const u16* first, const u16* second, u16* out);

void battle_effect_advance_coord_target_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames);

void battle_effect_advance_target_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames);

s32 battle_effect_calculate_launch_velocity(void);
s32 battle_effect_calculate_arc_trajectory_term(s32 a, s32 b);
s32 battle_effect_check_arc_trajectory_between_units(u8 source_id, u8 target_id);
s32 battle_effect_check_direct_trajectory_between_units(u8 source_id, u8 target_id);
void battle_effect_convert_svector_to_vector(s16* src, VECTOR* dst);
void battle_effect_convert_tile_coords_to_world_coords(s16* in, s32* out);
void battle_effect_copy_misc_unit_screen_location(u32 misc_id, VECTOR* destination);
void battle_effect_copy_svector(u16* src, u16* dst);
void battle_effect_draw_item_drop_sprite(VECTOR* origin, s32 item_id, s32 angle);

void battle_effect_draw_projectile_model(battle_effect_rotation_vector_t* spin, VECTOR* position,
    battle_effect_rotation_vector_t* angles, VECTOR* scale, s32 parameter);

s32 battle_effect_end_secondary_on_current_unit(void);
void battle_effect_find_projectile_xyz_along_trajectory(const VECTOR* src, s32 total, s32 current, VECTOR* dst);
s32 battle_effect_get_animation_timing_curve_value(battle_effect_timing_entry_t* entry);
s32 battle_effect_get_timing_curve_value(battle_effect_timing_entry_t* entry);
s32 battle_effect_init_data(s32 result);
void battle_effect_init_parameter_set(effect_list_node_t* state, s32 frame_group, s32 animation_index);
void battle_effect_init_trap_animation(s32 group, u16 palette_id, battle_effect_slot_t* slot);
s32 battle_effect_interpolate_s32_cos(s32 start, s32 end, s32 duration, s32 current);
u32 battle_effect_load_ability(s32 ability_id);
void battle_effect_render_particle_sprite(effect_list_node_t* particle, s32 red, s32 green, s32 blue);

void battle_effect_resolve_target_render_coords(
    s32 add_depth, s32 target_index, battle_effect_coord_data_t* coord_data, SVECTOR* out);

void battle_effect_run_keyframe_actions(u16 flags, u8 target, battle_keyframe_effect_state_t* state);
void battle_effect_run_keyframe_actions_all_targets(u16 flags, battle_keyframe_effect_state_t* state);
s32 battle_effect_set_secondary_teleport_by_misc_id(u32 misc_id);
s32 battle_effect_set_and_validate_arc_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);
void battle_effect_start_new(s32 effect_id, s32 target_mode, u8* target_data);
void battle_effect_store_map_center_coordinates(VECTOR* destination);

void battle_effect_submit_sprite_to_ordering_table(
    battle_effect_sprite_block_t* set, s16* position, s16 angle, VECTOR* zoom, u32* ot);

/* Steps the projectile along its trajectory, testing each obstacle. */
s32 battle_effect_trace_projectile_path(VECTOR* delta, SVECTOR* origin, s32* distance, void* obstacles);
void battle_effect_update_on_hit_sound_timer(u8* schedule, s16* entry_index, s16* countdown);
void battle_effect_add_vector_and_store_q12(const s32* delta, s32* value, s32* out);
s32 battle_effect_get_random_between(s32 value_a, s32 value_b);
void battle_effect_interpolate_svector_cos(const s16* from, const s16* to, s32 duration, s32 current, s16* out);
void battle_effect_interpolate_svector_cos_q12(const s16* from, const s16* to, s32 duration, s32 current, s16* out);
void battle_effect_interpolate_svector_linear(const s16* from, const s16* to, s32 duration, s32 current, s16* out);
void battle_effect_interpolate_vector_q13_cos(const s32* from, const s32* to, s32 duration, s32 current, s32* out);
void battle_effect_interpolate_vector_q13_cos_q12(const s32* from, const s32* to, s32 duration, s32 current, s32* out);
void battle_effect_interpolate_vector_q13_linear(const s32* from, const s32* to, s32 duration, s32 current, s32* out);

void battle_effect_add_random_rotation_offsets(const battle_effect_rotation_vector_t* source,
    const battle_effect_rotation_vector_t* ranges, battle_effect_rotation_vector_t* destination);

void battle_effect_add_rotation_vectors_mod_q12(const battle_effect_rotation_vector_t* first,
    const battle_effect_rotation_vector_t* second, battle_effect_rotation_vector_t* out);

void battle_effect_advance_caster_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames, s32 caster_misc_id);

effect_list_node_t* battle_effect_alloc_list_node(s16 kind);
void* battle_effect_alloc_primitive(s32 primitive_type);
s32 battle_effect_alloc_slot(void);
battle_effect_sprite_block_t* battle_effect_alloc_sprite_block(u8 sprite_count);
s32 battle_effect_alloc_state_record(void);
effect_work_record_t* battle_effect_alloc_work(s16 record_index, s32 kind);
void battle_effect_reset_child_timeline(battle_effect_temp_data_t* data, s16 value);
void battle_effect_free_list_node(effect_list_node_t* node);
void battle_effect_free_particle(s16 record_index, effect_work_record_t* work);
void battle_effect_free_slot(u8 id);
void battle_effect_free_sprite_block(battle_effect_sprite_block_t* block);
void* battle_effect_get_file_pointer(s16 effect_id);
effect_work_record_t* battle_effect_get_list_node(s16 record_index, s16 requested_index);
s32 battle_effect_get_phase(void);
s32 battle_effect_get_prim_buffer_offset(void);
void battle_effect_init_free_list(void);
void battle_effect_init_record_chain(void);
void battle_effect_init_render_state(void);
void battle_effect_init_state_records(void);
s16 battle_effect_start_script_record(u8* script, s16 start_pc, s16 parent_index);
void battle_effect_interpolate_emitter_spawn_spread(const void* source, s32 factor, s32* out);
s32 battle_effect_lerp_linear(s32 start, s32 end, s32 factor);
void battle_effect_play(void);
s32 battle_effect_run_block(s16 index);
s16 battle_effect_select_larger_magnitude_s16(s16 first, s16 second);
s16 battle_effect_select_largest_magnitude_s16(s16 first, s16 second, s16 third);
void battle_effect_set_phase_none(void);
void battle_effect_start_render_frame(void);
s32 battle_effect_update_cycle(void);
s32 battle_effect_update_slot(s32 id);
s32 battle_effect_update_stage(void);
void battle_effect_interpolate_emitter_spawn_position(const void* source, s32 factor, s32* out);
void battle_effect_interpolate_emitter_velocity_base_angles(const void* source, s32 factor, s32* out);
void battle_effect_interpolate_emitter_velocity_direction_spread(const void* source, s32 factor, s32* out);
void battle_effect_add_vectors_to_svector(VECTOR* left, VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_270(VECTOR* left, VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_180(VECTOR* left, VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_90(VECTOR* left, VECTOR* right, SVECTOR* result);
void battle_effect_draw_billboard_sprite(battle_effect_sprite_slot_t* slot);

/* turn */
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

/* One 4-byte entry of the 40-entry AT list built by battle_action_calculate_at_list and
 * kept in key order by battle_action_sort_at_list. */
typedef struct battle_at_entry {
    u8 unit;      /* 0x00; unit id, biased by the entry kind */
    u8 flags;     /* 0x01; low byte of the charging ability id */
    u16 sort_key; /* 0x02; insertion key for battle_action_sort_at_list */
} battle_at_entry_t;
typedef char battle_at_entry_size_must_be_4[(sizeof(battle_at_entry_t) == 4) ? 1 : -1];

/* Provisional 0xe-byte active-unit record at 0x8014d080, saved and restored
 * whole by battle_action_copy_at_and_cursor_to and
 * battle_action_copy_active_turn_data_from.
 * battle_menu_store_units_small_in_battle_display_data fills job_id..zodiac
 * for the unit named by battle_id. */
typedef struct battle_active_unit {
    s16 _unused_00; /* 0x00 */
    s16 job_id;     /* 0x02 */
    s16 brave;      /* 0x04 */
    s16 faith;      /* 0x06 */
    s16 zodiac;     /* 0x08 */
    s16 battle_id;  /* 0x0a: selected unit */
    s16 _unused_0c; /* 0x0c */
} battle_active_unit_t;
typedef char battle_active_unit_size_must_be_0xe[(sizeof(battle_active_unit_t) == 0xe) ? 1 : -1];

extern u8 g_acting_unit_elevation;
extern u8 g_acting_unit_x;
extern u8 g_acting_unit_y;
extern s32 g_battle_acting_unit_id;
extern u8 g_battle_acting_unit_id_byte;
extern battle_active_unit_t g_battle_active_turn_unit;
extern s32 g_battle_between_turn_resume_state;
extern s16 g_battle_acting_unit_used_ability_id;
extern s32 g_battle_between_turn_state;
extern s32 g_battle_turn_unit_id;

/* unit */
/* Miscellaneous Unit Data 0x130 identifies which role 0x131 links. */
typedef enum battle_misc_mount_state {
    BATTLE_MISC_MOUNT_STATE_NONE = 0,
    BATTLE_MISC_MOUNT_STATE_RIDER = 1,
    BATTLE_MISC_MOUNT_STATE_MOUNT = 2,
} battle_misc_mount_state_e;

/* Battle Stats 0x182 links the two simulation records in a mounted pair. */
typedef enum battle_mount_info_flags {
    BATTLE_MOUNT_INFO_PARTNER_ID_MASK = 0x1f,
    BATTLE_MOUNT_INFO_FLAG_MOUNT = 0x40,
    BATTLE_MOUNT_INFO_FLAG_RIDER = 0x80,
    BATTLE_MOUNT_INFO_ROLE_MASK = 0xc0,
} battle_mount_info_flags_e;

/* Lifecycle states stored in Battle Stats 0x183. Zero and 0xff participate in
 * broader absence/sentinel checks and remain deliberately unnamed. */
typedef enum battle_unit_existence_state {
    BATTLE_UNIT_EXISTENCE_ACTIVE = 1,
    BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL = 2,
    BATTLE_UNIT_EXISTENCE_DISABLED = 0x80,
} battle_unit_existence_state_e;

/* Miscellaneous Unit Data 0x13f is XORed with the sprite display flags. */
typedef enum battle_misc_display_flags {
    BATTLE_MISC_DISPLAY_HORIZONTAL_FLIP = 0x02,
    BATTLE_MISC_DISPLAY_VERTICAL_FLIP = 0x04,
} battle_misc_display_flags_e;

typedef enum battle_unit_equipped_flags {
    BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE = 0x04,
    BATTLE_UNIT_EQUIPPED_FLAG_SWORD = 0x08,
} battle_unit_equipped_flags_e;

typedef enum battle_unit_height_flags {
    BATTLE_UNIT_HEIGHT_NON_CHOCOBO = 0x20,
    BATTLE_UNIT_HEIGHT_SHARED_TILE = 0x40,
    BATTLE_UNIT_HEIGHT_UNAVAILABLE = 0x80,
} battle_unit_height_flags_e;

/* Selection policy for battle_unit_find_at_tile.  Status bits are inclusive
 * alternatives; the Chocobo bit is an additional requirement. */
typedef enum battle_unit_tile_filter {
    BATTLE_UNIT_TILE_FILTER_TREASURE = 0x01,
    BATTLE_UNIT_TILE_FILTER_DRY_LOW_SLOPE = 0x02,
    BATTLE_UNIT_TILE_FILTER_TRANSPARENT = 0x10,
    BATTLE_UNIT_TILE_FILTER_CRYSTAL = 0x40,
    BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO = 0x80,
    BATTLE_UNIT_TILE_STATUS_FILTERS
    = BATTLE_UNIT_TILE_FILTER_TREASURE | BATTLE_UNIT_TILE_FILTER_TRANSPARENT | BATTLE_UNIT_TILE_FILTER_CRYSTAL,
} battle_unit_tile_filter_e;

/* battle_stats_t.entd_slot value for an unused or removed slot. */
enum {
    BATTLE_ENTD_SLOT_NONE = 0xff,
};

/* First SHP-style section of a VRAM cache slot: two header words, frame
 * pointers and the frame data they point into (battle_gfx_unpack_evtchr_file_to_vram_slot relocates
 * the file's frame offsets into these pointers). */
typedef struct battle_gfx_shp0 {
    s32 header[2];    /* 0x000 */
    u8* frames[0x40]; /* 0x008 */
    u8 data[0x400];   /* 0x108 */
} battle_gfx_shp0_t;

/* One 0x30-byte sprite animation state. Misc unit data holds four from
 * 0x1d8: the unit sprite's own state (spelled out field by field in
 * battle_unit_misc_data_t) and three weapon/effect battle_unit_sprite_block_t
 * records. The first 0x24 bytes are common to both; the unit state keeps its
 * saved SHP/SEQ pointers at 0x24/0x28 and its display pointer at 0x2c. */
typedef struct battle_unit_anim_state {
    u16 trigger;                               /* 0x00 */
    u16 graphic_type;                          /* 0x02; 0 for the unit sprite (battle_unit_init_misc_data) */
    u16 animation_id;                          /* 0x04 */
    u16 script_pos;                            /* 0x06 */
    u16 frame;                                 /* 0x08 */
    u16 wait;                                  /* 0x0a */
    u16 counter_0c;                            /* 0x0c */
    u16 saved_animation_id;                    /* 0x0e */
    u16 saved_script_pos;                      /* 0x10 */
    u16 wait_bias;                             /* 0x12 */
    u16 frame_bias;                            /* 0x14 */
    u16 loop_count;                            /* 0x16 */
    u16 flags;                                 /* 0x18 */
    u8 _padding_1a[2];                         /* 0x1a; aligns shp */
    s32* shp;                                  /* 0x1c; SHP frame table: 0x008 normal, 0x348 submerged */
    struct battle_gfx_shp0* seq;               /* 0x20 */
    u8* saved_shp;                             /* 0x24 */
    u8* saved_seq;                             /* 0x28 */
    battle_gfx_sprite_display_data_t* display; /* 0x2c */
} battle_unit_anim_state_t;

/* Renderer-side Misc-unit status mirror. These packed masks do not use the
 * canonical battle_status_id_e byte order. */
typedef enum battle_misc_status_flags_1_4 {
    /* Misc +0x140. Bit 0x00000001 remains unknown. */
    BATTLE_MISC_STATUS_CONFUSION = 0x00000080,
    BATTLE_MISC_STATUS_SLEEP = 0x00000040,
    BATTLE_MISC_STATUS_PETRIFY = 0x00000020,
    BATTLE_MISC_STATUS_STOP = 0x00000010,
    BATTLE_MISC_STATUS_CRITICAL = 0x00000008,
    BATTLE_MISC_STATUS_DEAD = 0x00000004,
    BATTLE_MISC_STATUS_MOUNTED = 0x00000002,

    /* Misc +0x141. */
    BATTLE_MISC_STATUS_FAITH = 0x00008000,
    BATTLE_MISC_STATUS_BERSERK = 0x00004000,
    BATTLE_MISC_STATUS_CURSED = 0x00002000,
    BATTLE_MISC_STATUS_HASTE = 0x00001000,
    BATTLE_MISC_STATUS_SLOW = 0x00000800,
    BATTLE_MISC_STATUS_DEFENDING = 0x00000400,
    BATTLE_MISC_STATUS_CHARGING = 0x00000200,
    BATTLE_MISC_STATUS_PERFORMING = 0x00000100,

    /* Misc +0x142. */
    BATTLE_MISC_STATUS_OIL = 0x00800000,
    BATTLE_MISC_STATUS_POISON = 0x00400000,
    BATTLE_MISC_STATUS_PROTECT = 0x00200000,
    BATTLE_MISC_STATUS_SHELL = 0x00100000,
    BATTLE_MISC_STATUS_DONT_ACT = 0x00080000,
    BATTLE_MISC_STATUS_DONT_MOVE = 0x00040000,
    BATTLE_MISC_STATUS_BLOOD_SUCK = 0x00020000,
    BATTLE_MISC_STATUS_INNOCENT = 0x00010000,

    /* Misc +0x143. Bit 0x80000000 remains unknown. */
    BATTLE_MISC_STATUS_UNDEAD = 0x40000000,
    BATTLE_MISC_STATUS_REGEN = 0x20000000,
    BATTLE_MISC_STATUS_RERAISE = 0x10000000,
    BATTLE_MISC_STATUS_DARKNESS = 0x08000000,
    BATTLE_MISC_STATUS_DEATH_SENTENCE = 0x04000000,
    BATTLE_MISC_STATUS_SILENCE = 0x02000000,
    BATTLE_MISC_STATUS_CHARM = 0x01000000,

    BATTLE_MISC_STATUS_PALETTE_MOD_MASK = 0x60c26020,
    BATTLE_MISC_STATUS_BUBBLE_MASK = 0x1fffc2c4,
    BATTLE_MISC_STATUS_PREVENT_FACING_MASK = 0x00080074,
    BATTLE_MISC_STATUS_ANIMATION_SELECTION_MASK = 0x00003fff,
} battle_misc_status_flags_1_4_e;

typedef enum battle_misc_status_flags_5_6 {
    /* Misc +0x144. */
    BATTLE_MISC_STATUS_CRYSTAL = 0x00000001,
    BATTLE_MISC_STATUS_CHICKEN = 0x00000002,
    BATTLE_MISC_STATUS_FROG = 0x00000004,
    BATTLE_MISC_STATUS_TREASURE = 0x00000008,
    BATTLE_MISC_STATUS_TRANSPARENT = 0x00000010,
    BATTLE_MISC_STATUS_FLOAT = 0x00000020,
    BATTLE_MISC_STATUS_JUMP = 0x00000040,
    BATTLE_MISC_STATUS_POACHED = 0x00000080,

    /* Misc +0x145. Other bits remain unknown. */
    BATTLE_MISC_STATUS_MORBOL = 0x00000100,
    BATTLE_MISC_STATUS_JUMP_HEIGHT_ACTIVE = 0x00000200,

    BATTLE_MISC_STATUS_TRANSFORMATION_MASK = 0x0000000f,
} battle_misc_status_flags_5_6_e;

/* Kept while source files migrate to the more specific type name. */
/* In-use words at 0x800b7304, stride 0x440: each sits one word ahead of its
 * g_battle_unit_misc_data record, in the previous record's last word. */
typedef struct battle_unit_misc_slot_flag {
    s32 in_use;
    u8 _unused_004[0x43c];
} battle_unit_misc_slot_flag_t;
typedef char battle_misc_slot_flag_size_must_be_0x440[(sizeof(battle_unit_misc_slot_flag_t) == 0x440) ? 1 : -1];

/* Provisional: war-trophy search result filled by
 * battle_unit_find_war_trophies_and_bonus_money. REQUIRE links the same routine
 * as battle_unit_find_war_trophies_and_bonus_money into
 * g_require_reward_war_trophy_search_result. */
typedef struct battle_war_result {
    u8 item_ids[BATTLE_UNIT_SLOT_COUNT]; /* 0x00 */
    u8 item_count;                       /* 0x15 */
    u8 _padding_16[2];                   /* 0x16; aligns bonus_money */
    s32 bonus_money;                     /* 0x18; sum of bonus-money modifiers * 100 */
    s32 level_sum_money;                 /* 0x1c; g_enemy_level_sum * 100 */
    s32 level_bonus_money;               /* 0x20; (highest enemy level - lowest level) * 100 */
} battle_war_result_t;
typedef char battle_war_result_size_must_be_0x24[(sizeof(battle_war_result_t) == 0x24) ? 1 : -1];

typedef struct battle_unit_height_data {
    u8 x;
    u8 y;
    u8 elevation;
    u8 unit_flags;
    u8 unit_height;
    u8 walking_height;
    u8 total_height;
    u8 _unused_07; /* not written by battle_calculate_unit_height_data */
} battle_unit_height_data_t;
typedef char battle_unit_height_data_size_must_be_8[sizeof(battle_unit_height_data_t) == 8 ? 1 : -1];

/* Provisional: one gauge of the status record. */
typedef struct battle_unit_status_gauge {
    s16 value;  /* 0x00 */
    s16 value2; /* 0x02 */
    s16 max;    /* 0x04 */
} battle_unit_status_gauge_t;

enum {
    BATTLE_UNIT_STATUS_GAUGE_HP = 0,
    BATTLE_UNIT_STATUS_GAUGE_MP = 1,
    BATTLE_UNIT_STATUS_GAUGE_CT = 2,
};

/* Provisional 0x22-byte unit status record: the AT banner at 0x8014d038 and the
 * status billboard at 0x8014d05c. battle_menu_copy_unit_data_to_status_billboard
 * (and its DEBUGCHR twin) fills the first 0x1e bytes,
 * battle_menu_display_hovered_unit_stats draws it, and
 * battle_action_copy_at_and_cursor_to saves the banner whole as 0x22 bytes. */
typedef struct battle_unit_status_record {
    s16 level;                            /* 0x00 */
    s16 team_kind;                        /* 0x02: 0-3 from team flags, 3 = auto-battle; selects the status icon */
    s16 list_index;                       /* 0x04: AT-list position + 1, negative when the unit is not listed */
    s16 unit_count;                       /* 0x06 */
    s16 experience;                       /* 0x08 */
    s16 battle_id;                        /* 0x0a */
    battle_unit_status_gauge_t gauges[3]; /* 0x0c: BATTLE_UNIT_STATUS_GAUGE_*; CT max is always 100 */
    s16 projected_display_value;          /* 0x1e */
    u16 projected_hit_percent;            /* 0x20 */
} battle_unit_status_record_t;
typedef char battle_unit_status_record_size_must_be_0x22[(sizeof(battle_unit_status_record_t) == 0x22) ? 1 : -1];

/* Sprite lookup result filled in by battle_gfx_get_unit_sprite_frame_and_vram_ids. */
typedef struct {
    s16 vram_palette_id;     /* 0x00 */
    s16 vram_spritesheet_id; /* 0x02 */
    s16 graphic_height;      /* 0x04 */
    s16 graphic_y_offset;    /* 0x06 */
} battle_unit_sprite_query_t;

/* Queued unit graphics load (g_unit_graphics_load_descriptors, 16 entries),
 * appended by battle_gfx_append_unit_graphics_load_descriptor and consumed by
 * the per-unit graphics loader at 0x80088904. */
typedef struct battle_unit_graphics_load_descriptor {
    u8 map_x;
    u8 map_y;
    u8 map_level;
    u8 _padding_03; /* aligns map_height */
    u16 map_height;
    s16 portrait_id;
    u16 palette_id;
    u16 misc_id;
    struct battle_stats* battle_stats;
    u32 flags;
} battle_unit_graphics_load_descriptor_t;
typedef char battle_unit_graphics_load_descriptor_size_must_be_0x14
    [(sizeof(battle_unit_graphics_load_descriptor_t) == 0x14) ? 1 : -1];

typedef enum battle_unit_spell_quote_skillset_flags {
    BATTLE_UNIT_SPELL_QUOTE_SECONDARY_SKILLSET = 1,
    BATTLE_UNIT_SPELL_QUOTE_PRIMARY_SKILLSET = 2,
} battle_unit_spell_quote_skillset_flags_e;

enum {
    BATTLE_UNIT_POSITION_STEPPING_STONE = 0x4000,
};

extern const u32 g_battle_misc_status_mask_by_handler_index[BATTLE_STATUS_COUNT + 1];
extern void* g_battle_unit_last_misc_init_byte;
extern battle_unit_misc_data_t g_battle_unit_misc_data[16];
extern battle_unit_misc_slot_flag_t g_battle_unit_misc_slot_flags[];
extern unit_status_staging_t* g_battle_unit_status_staging_data;
extern s32 g_battle_unit_palette_animation_offset;
extern s32 g_battle_unit_palette_animation_timer;
extern battle_unit_misc_data_t* g_battle_misc_unit_list_head;
extern u16 g_battle_unit_view_started_battle_id;
extern u16 g_battle_unit_view_started_comparison_id;
extern s32 g_battle_unit_view_mode;
extern u16 g_battle_unit_status_bounce_step;
extern world_gfx_image_load_parameters_t g_battle_unit_editor_sprite_image_params_a[19];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_sprite_image_params_b[19];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_row_image_params_a[4];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_row_image_params_b[4];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_item_icon_image_params[5];
extern u16 g_battle_unit_editor_sprite_clut_pairs[19][2];
extern world_menu_palette_slot_t g_battle_unit_summary_palettes[];
extern u8 g_battle_unit_summary_digit_images[];
extern world_menu_palette_primitives_t g_battle_unit_editor_palette;
extern s32 g_battle_companion_overlay_primitives;
extern u8 g_battle_rider_status_infliction_mask[5];
extern s32 g_battle_entd_selection_mode;

/* Count and x/y/level byte triples of the tiles already handed out as dismount
 * destinations this action. */
extern s32 g_battle_relocated_unit_count;
extern u8 g_battle_relocated_unit_coords[][3];
extern RECT g_battle_unit_editor_cursor_rect; /* cursor source */
extern RECT g_battle_unit_editor_panel_rect;  /* panel box */
extern RECT g_battle_unit_editor_text_rect_a; /* first text image */
extern RECT g_battle_unit_editor_text_rect_b; /* second text image */
extern s32 g_battle_unit_graphics_load_complete;
extern battle_unit_misc_data_t* g_battle_unit_last_misc_data;
extern battle_unit_misc_data_t* g_battle_unit_misc_list_head;

/* Per-unit pending animation id, applied once the unit stops moving. */
extern s16 g_battle_unit_pending_animation_ids[];
extern battle_stats_t g_battle_unit_stats[BATTLE_UNIT_SLOT_COUNT];
extern u16 g_battle_unit_status_bounce_offsets[];
extern u8 g_battle_unit_status_first_page_by_mode[];
extern CVECTOR g_battle_unit_status_gauge_colors[];
extern world_gfx_image_load_parameters_t g_battle_unit_status_icon_image_params[];
extern u16 g_battle_unit_status_quad_clut_pairs[][2];
extern world_gfx_image_load_parameters_t* g_battle_unit_status_quad_image_params_by_mode[];
extern u16 g_battle_unit_status_sprite_clut_pairs[][2];
extern world_gfx_image_load_parameters_t* g_battle_unit_status_sprite_image_params_by_mode[];
extern RECT g_battle_unit_summary_panel_rect;
extern world_gfx_image_load_parameters_t g_battle_unit_summary_portrait_image_params;
extern world_gfx_image_load_parameters_t g_battle_unit_summary_sprite_image_params[];
extern s16 g_battle_unit_was_moving_latch[];
extern s32 g_companion_overlay_state;
extern battle_stats_t battle_stats[BATTLE_UNIT_SLOT_COUNT];
extern battle_unit_graphics_load_descriptor_t g_unit_graphics_load_descriptors[];

/* BATTLE and WORLD use the same seven-byte per-unit rotation state. */
extern world_unit_animation_state_t g_battle_unit_misc_rotation_data[];

/* 0x8016604e: result of battle_gfx_poll_unit_graphics_load, 0 done / 1 pending. */
extern u16 g_battle_unit_graphics_load_pending;
extern world_unit_editor_panel_data_t g_battle_unit_editor_panel_data;
extern world_unit_editor_panel_data_t g_battle_unit_comparison_editor_panel_data;
extern u8 g_battle_unit_editor_line_colors[8 * WORLD_UNIT_EDITOR_LINE_SIZE];
extern s32 g_unit_graphics_load_count;
battle_unit_height_data_t* battle_calculate_unit_height_data(battle_unit_height_data_t* out, s32 unit_id);
battle_stats_t* battle_find_unit_data_pointer_for_entd_unit_id(s32 unit_id, s32* out_battle_id);
battle_stats_t* battle_unit_get_existing_pointer(u32 unit_id);
s32 battle_get_misc_id(s32 unit_id);
void battle_load_entd_units(entd_encounter_t* entd_data, s32 mode);
s32 battle_unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters);
s32 battle_unit_animate_and_set_enemy_level_data_by_misc_id(u32 misc_id);
void battle_unit_apply_poach_morbol_transformation(battle_unit_misc_data_t* unit);
void battle_unit_apply_staged_status_data(s32 unit_id, s32 misc_id);
u8 battle_unit_calculate_move_exp_jp_up_to_earn(battle_stats_t* unit);
void battle_unit_sync_all_rider_mount_positions(void);
void battle_unit_clear_distortion_animation(battle_unit_misc_data_t* unit);
void battle_unit_clear_distortion_animation_2(battle_unit_misc_data_t* unit);
void battle_unit_clear_facing_update_suppression(u32 misc_id);
void battle_unit_clear_status_staging_data(void);
void battle_unit_copy_misc_screen_location(u32 misc_id, VECTOR* destination);
void battle_unit_copy_rider_data_to_mount(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* rider);
s32 battle_unit_count_crystal_misc_units(void);
void battle_unit_decide_facing_direction(battle_unit_misc_data_t* unit, s32 facing);
void battle_unit_dismount_rider(battle_unit_misc_data_t* unit);
void battle_unit_dispatch_distortion_animation(battle_unit_misc_data_t* unit);
void battle_unit_face_toward_cursor(battle_unit_misc_data_t* unit);
battle_unit_misc_data_t* battle_unit_find_other_chocobo_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
void battle_unit_find_war_trophies_and_bonus_money(battle_war_result_t* result);
s32 battle_unit_disable_remove(s32 misc_id);
void battle_unit_init_for_store_anim_facing_move(s32 misc_id, s32 value);
void battle_unit_project_misc_to_screen(u32 misc_id, s16* screen_coordinates);
s32 battle_unit_set_enemy_level_data_by_battle_id(u32 battle_id);
s32 battle_unit_set_target_anim_by_misc_id(u16 misc_id);
void battle_unit_set_facing_update_suppression(u32 misc_id);
s32 battle_unit_get_battle_index_by_entd_unit_id(s32 unit_id);
s32 battle_unit_get_battle_index_by_misc_id(u32 misc_id);
battle_stats_t* battle_unit_get_by_unit_id(s32 unit_id);
battle_unit_misc_data_t* battle_unit_get_casting_misc_data(void);
u8 battle_unit_get_effective_height(battle_stats_t* unit);
s16 battle_unit_get_facing_quadrant_by_misc_id(u32 misc_id);
battle_unit_misc_data_t* battle_unit_get_selectable_misc_data_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
battle_unit_misc_data_t* battle_unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
battle_unit_misc_data_t* battle_unit_get_misc_data_by_battle_id(u32 battle_id);
battle_unit_misc_data_t* battle_unit_get_misc_data_by_misc_id(u32 misc_id);
s32 battle_unit_get_misc_id_by_battle_id(u32 battle_id);
battle_unit_misc_data_t* battle_unit_get_source_misc_data(void);
battle_stats_t* battle_unit_get_stats_from_battle_id(u32 id);
s32 battle_unit_has_misc_id(u32 misc_id);
void battle_unit_increase_blue_team_brave_by_10(void);
s32 battle_unit_init_party_member_battle_data(s32 unused0, s32 unused1, s32 unit_index);
void battle_unit_learn_used_ability(battle_stats_t* unit);
void battle_unit_modify_entd_unit(battle_stats_t* unit);
void battle_unit_mount_rider_onto_unit(battle_unit_misc_data_t* rider, battle_unit_misc_data_t* mount);
s32 battle_unit_move_misc_unit_to_head(u32 misc_id);
s32 battle_unit_place_in_new_location(u32 misc_id, s16* tile, s32 facing);
s32 battle_unit_poll_and_load_entd_units(void);
s32 battle_unit_remove_misc_by_id(u32 misc_id);
void battle_unit_reset_animation_states(void);
void battle_unit_save_previous_state(battle_unit_misc_data_t* unit);
void battle_unit_set_animation_based_on_mounted_state(battle_unit_misc_data_t* unit);
void battle_unit_set_bow_hardcoding(battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);
s32 battle_unit_set_cannot_exist(s32 unit_index);
void battle_unit_set_enemy_level_data(battle_stats_t* unit);
void battle_unit_set_animation_based_on_status(battle_unit_misc_data_t* unit);
void battle_unit_set_idle_animation_for_movement(battle_unit_misc_data_t* unit);
void battle_unit_set_level_up_animation(battle_unit_misc_data_t* unit);
void battle_unit_set_map_coords_after_death_dismount(battle_unit_misc_data_t* unit);
void battle_unit_set_real_coords_from_map_coords(battle_unit_misc_data_t* unit);
void battle_unit_set_rider_mount_x_y_facing(battle_unit_misc_data_t* unit);
void battle_unit_set_specific_animation_value_on_battle_init(s32 misc_id, u32 value);
void battle_unit_snap_all_facings_to_quadrants(void);
void battle_unit_start_ability_charge_animation(battle_unit_misc_data_t* unit);
void battle_unit_start_ability_charge_animation_for_movement(battle_unit_misc_data_t* unit);
void battle_unit_store_animation_facing(u32 animation, s32 facing, battle_unit_misc_data_t* unit);
void battle_unit_store_coordinate_mount_animation_data(battle_unit_misc_data_t* unit);
s32 battle_unit_try_get_misc_data_by_unit_id(u16* unit_id, s32* state);
void battle_unit_save_facing_before_attack(battle_unit_misc_data_t* unit);
void battle_unit_update_and_animate_units(void);
void battle_unit_update_anim_display_for_all_targets(battle_unit_misc_data_t* unit);
void battle_unit_update_attacker_earned_experience(s32 outcome);
void battle_unit_update_display(battle_unit_misc_data_t* unit);
void battle_unit_update_display_by_misc_id(u16 misc_id);
s32 battle_unit_update_fade_out_removal(battle_unit_misc_data_t* unit);
void battle_unit_update_float_and_jump_height(battle_unit_misc_data_t* unit);
void battle_unit_update_graphics_by_misc_id_wrapper(u32 misc_id);
void battle_unit_update_mount_animation_display(battle_unit_misc_data_t* unit);
void battle_unit_update_post_command_animation_display(battle_unit_misc_data_t* unit);
void battle_unit_update_staged_status_data(u16 unit_id, u16 restore);
s32 battle_unit_set_placement_and_validate(s32 unit_index, battle_deployed_coords_t* src);
s32 battle_unit_validate_placement(s32 unit_index);
void battle_unit_view_supervisor_thread(void);
s32 battle_update_unit_status_and_staged_status_data();
s32 battle_unit_find_relocation_tile(s32 unit_index, battle_dismount_coords_t* destination);
void battle_unit_update_animation_for_status_changes(battle_unit_misc_data_t* unit);
void battle_unit_move_toward_action_target(battle_unit_misc_data_t* unit, s32 allow_height_change);
s32 battle_unit_apply_action_rewards(s32 unit_index, battle_action_reward_display_t* display);
s32 battle_unit_init_deployed_units_data(battle_deployed_coords_t* formation, s32 is_red_team_debug);
s32 battle_unit_set_tile_position(s32 misc_unit_id, u8 map_x, u8 map_y, u8 map_z, u8 facing);

void battle_unit_set_target_animation_from_attack_type(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);

void update_unit_action_statuses(battle_stats_t* unit, s32 action_state);
s16 battle_unit_get_camera_facing_quadrant_by_battle_id(u32 battle_id);
void battle_unit_add_signed_byte_to_height(battle_unit_misc_data_t* unit, s32 delta);
s32 battle_unit_apply_level_up_down_ability(void);
s32 battle_unit_apply_stat_increment_decrement(s32 mod, u8* stat, u8 max, u8 min);
s32 battle_unit_build_deployed_units_data(s32 a0);
s32 battle_unit_build_gained_exp_jp_level_job_level(battle_stats_t* unit, battle_action_reward_display_t* rewards);
void battle_unit_call_bow_hardcoding_by_misc_id(u16 attacker_id, u16 target_id);
void battle_unit_call_set_animation_based_on_status(struct battle_unit_misc_data* unit);
s32 battle_unit_check_chocobo(battle_stats_t* unit);
s32 battle_unit_check_facing_relative_to_camera(s32 unit_id);
battle_unit_misc_data_t* battle_unit_claim_misc_slot(s16 unit_id);
void battle_unit_clear_horizontal_flip_flag(u32 misc_id);
void battle_unit_clear_movement_effect_suppression(u32 misc_id);
void battle_unit_clear_palette_update_suppression(u32 misc_id);
void battle_unit_copy_map_coords_from_misc_id(u32 misc_id, s16* coords);
void battle_unit_copy_misc_data_xyz_values(SVECTOR* destination, const battle_unit_misc_data_t* misc_data);
void battle_unit_copy_misc_screen_coords(u32 misc_id, u16* screen_coords);
void battle_unit_dismount_rider_and_update_display(battle_unit_misc_data_t* rider);
void battle_unit_face_towards_action_target(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* target);
battle_stats_t* battle_unit_find_active_data_pointer(void);
s16* battle_unit_get_event_offset_ptr_by_misc_id(u32 misc_id);
battle_stats_t* battle_unit_get_attacker_data_pointer(void);
s32 battle_unit_get_facing_nibble_by_misc_id(u32 misc_id);
s32 battle_unit_get_map_coords_from_misc_id(u32 misc_id, s16* coords);
battle_unit_misc_data_t* battle_unit_find_any_misc_data_at_map_coords(s32 map_x, s32 map_y, s32 map_z);

battle_unit_misc_data_t* battle_unit_get_overlapping_misc_data_pointer(
    battle_unit_misc_data_t* self, s32 x, s32 y, s32 z);

battle_ai_command_action_t* battle_unit_get_target_id_ptr_by_battle_id(u32 battle_id);
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, battle_screen_coords_t* coords);
s32 battle_unit_add_event_offset_by_misc_id(u32 misc_id, const battle_screen_coords_t* amount);
void battle_unit_increment_or_decrement_height_mod(battle_unit_misc_data_t* unit);
void battle_unit_init_coordinates(struct battle_unit_misc_data* unit);
void battle_unit_init_coordinates_animation_facing(battle_unit_misc_data_t* unit);
void battle_unit_init_deployed_units_data_for_debug_red_team(s32 value);

battle_unit_misc_data_t* battle_unit_init_misc_data(s32 map_x, s32 map_y, s32 map_level, s16 facing, s32 spritesheet_id,
    s16 palette, s16 misc_id, battle_stats_t* stats, u32 flags, u8* data);

s32 battle_unit_learn_from_crystal(battle_stats_t* unit, s32 flags);
s32 battle_unit_load_job_level(battle_stats_t* unit, s32 job_id, s32* out_slot);
void battle_unit_mount_onto_pending_mount(battle_unit_misc_data_t* unit);
s32 battle_unit_prepare_rider_dismount(u32 misc_id, s32 direction, s32 jump_length);
void battle_unit_remove_misc(battle_unit_misc_data_t* unit);
s32 battle_unit_select_and_open_entd(void);

void battle_unit_select_attack_animation_for_ability(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);

void battle_unit_select_weapon_attack_animation(battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);
void battle_unit_set_animation_for_item_abilities(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* target);
s32 battle_unit_set_animation_from_facing_nibble(u32 misc_id, s32 facing_nibble);

void battle_unit_set_attacker_animation_for_shield_block(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);

void battle_unit_set_horizontal_flip_flag(u32 misc_id);
void battle_unit_set_mounted_animation_by_misc_id(u32 misc_id);
void battle_unit_set_move_and_screen_coords(battle_unit_misc_data_t* unit);
void battle_unit_set_movement_effect_suppression(u32 misc_id);
void battle_unit_set_palette_update_suppression(u32 misc_id);
void battle_unit_set_specific_animation_by_misc_id(u32 misc_id, u32 animation);
void battle_unit_set_status_animation_by_misc_id(u32 misc_id);
void battle_unit_shift_forward_or_backward(battle_unit_misc_data_t* unit, s32 delta);
void battle_unit_shift_right_left(battle_unit_misc_data_t* unit, s8 delta);
void battle_unit_store_animation_facing_movement_data(s32 animation, s32 facing, battle_unit_misc_data_t* unit);
void battle_unit_store_entd_flags_without_control_flag(battle_unit_misc_data_t* unit);
s32 battle_unit_store_jp_and_calculate_unlocked_jobs(s32 unit_index, s32 jp, s32 team, s32 slot);
void battle_unit_update_and_animate_units_three_times(void);
void battle_unit_update_attack_result_animation(battle_unit_misc_data_t* unit);
void battle_unit_update_facing_and_pending_animations(void);
void battle_unit_start_post_attack_animation_display(s32 flag, u8* ctx);
u8 battle_unit_generate_treasure(battle_stats_t* unit);
battle_screen_coords_t* battle_copy_misc_unit_screen_coords(u32 misc_id);
struct battle_unit_misc_data* battle_unit_get_mount_misc_data(struct battle_unit_misc_data* unit);
battle_screen_coords_t* battle_unit_get_screen_data_ptr_by_misc_id(u32 misc_id);
void battle_unit_set_real_coords_from_screen_coords(struct battle_unit_misc_data* unit);
void battle_unit_set_screen_coords_from_real_coords(struct battle_unit_misc_data* unit);
void battle_unit_clear_misc_units(void);

/* gfx */
/* 0x8013b6e4 initializes independent nine-tile strips at +0x1c and +0xac
 * in the same 0x13c-byte storage used by world_fade_tile_frame_t. */
typedef struct battle_scanline_frame {
    DR_MODE draw_mode; /* 0x00 */
    TILE center;       /* 0x0c */
    TILE backdrop[9];  /* 0x1c */
    TILE bar[9];       /* 0xac */
} battle_scanline_frame_t;
typedef char battle_scanline_frame_size_must_be_0x13c[(sizeof(battle_scanline_frame_t) == 0x13c) ? 1 : -1];

/* Commands of 0x800e77b8, which stores RGB and controls GTE background color. */
typedef enum battle_background_color_command {
    BATTLE_BACKGROUND_COLOR_SET = 9,
    BATTLE_BACKGROUND_COLOR_GET = 10,
    BATTLE_BACKGROUND_COLOR_DISABLE = 11,
    BATTLE_BACKGROUND_COLOR_RESTORE = 12
} battle_map_background_color_command_e;

/* Reviewed direct spritesheet IDs and exclusive range boundaries; the
 * remaining IDs are not yet a complete semantic domain. */
enum {
    BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1 = 0x01,
    BATTLE_SPRITESHEET_ID_RAMZA_END = 0x04,
    BATTLE_SPRITESHEET_ID_ITEM = 0x1e,
    BATTLE_SPRITESHEET_ID_FRAME = 0x1f,
    BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM = 0x41,
    BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM = 0x49,
    BATTLE_SPRITESHEET_ID_CHOCOBO = 0x86,
    BATTLE_SPRITESHEET_ID_SKELETON = 0x8b,
    BATTLE_SPRITESHEET_ID_GHOUL = 0x8c,
    BATTLE_SPRITESHEET_ID_MORBOL = 0x92,
    BATTLE_SPRITESHEET_ID_CRYSTAL = 0x9b,
    BATTLE_SPRITESHEET_ID_CHICKEN = 0x9c,
    BATTLE_SPRITESHEET_ID_FROG = 0x9d,
    BATTLE_SPRITESHEET_ID_TREASURE = 0x9e,
};

typedef struct battle_gfx_render_unit {
    u8 _unused_000[0x12];
    u16 sprite_flags;
    u8 _unused_014[0x10c];
    SVECTOR camera_relative_position;
    u32 otag_depth;
    u8 _unused_12c[0x1a6];
    s8 item_get_x_offset; /* 0x2d2; battle_unit_misc_data_t item_get_x_offset */
    s8 item_get_y_offset; /* 0x2d3 */
    u8 _unused_2d4[4];
    battle_gfx_sprite_display_data_t* found_item_display;
    u8 _unused_2dc[0x10];
    SVECTOR display_svectors[6];
} battle_gfx_render_unit_t;

/* Per-unit palette bookkeeping at 0x8009b27c, indexed by Misc Unit Data ID
 * with an eight-byte stride; only the leading gate byte is used. */
typedef struct battle_palette_update_entry {
    u8 pending; /* 0x00 */
    u8 _unused_01[7];
} battle_palette_update_entry_t;

/* Render view of the same sprite display header. The first two colour bytes
 * are read as one halfword, and texture/scale words as unsigned halfwords. */
typedef struct battle_gfx_sprite_display {
    u16 rg;
    u8 b;
    u8 _padding_03; /* aligns tpage */
    u16 tpage;
    u16 clut;
    u16 scale_x;
    u16 scale_y;
    u16 angle;
    battle_gfx_sprite_part_display_data_t parts[1];
} battle_gfx_sprite_display_t;

/* Second section, indexed from frame 0xd2 (battle_gfx_select_unit_shp_frame). Its frame
 * data is stored twice, once for each frame table; deep-water units use the
 * submerged half. */
typedef struct battle_gfx_shp1 {
    s32 header[2];              /* 0x000 */
    u8* frames[0xd0];           /* 0x008 */
    u8* submerged_frames[0xd0]; /* 0x348 */
    u8 data[0x1e0];             /* 0x688 */
    u8 submerged_data[0x1e0];   /* 0x868 */
} battle_gfx_shp1_t;

/*
 * Two of these records back the battle EVTCHR/SP2 VRAM cache.  The event
 * SaveEVTCHR and LoadEVTCHR commands establish the header fields and the
 * 0x6400-byte image payload; every target access uses a 0x7564-byte stride.
 */
typedef struct battle_gfx_vram_slot {
    u32 owner;              /* 0x0000; 0xff free, 0xfe saved */
    u32 evtchr_load_marker; /* 0x0004 */
    u32 evtchr_load_state;  /* 0x0008 */
    RECT image_rect;        /* 0x000c */
    battle_gfx_shp0_t shp0; /* 0x0014 */
    battle_gfx_shp1_t shp1; /* 0x051c */
    u16 cluts[16][16];      /* 0x0f64; filled by battle_gfx_set_evtchr_palette_by_misc_id */
    u8 image_data[0x6400];  /* 0x1164 */
} battle_gfx_vram_slot_t;
typedef char battle_vram_slot_size_must_be_0x7564[(sizeof(battle_gfx_vram_slot_t) == 0x7564) ? 1 : -1];

/* 0x32d6-byte spritesheet VRAM slot records at 0x800c7ce8 (nine slots);
 * g_battle_gfx_spritesheet_ids_by_vram_slot aliases the first record's byte 1. */
typedef struct battle_gfx_spritesheet_slot {
    u8 in_use;                /* 0x00; cleared when no live unit uses the slot */
    u8 spritesheet_id;        /* 0x01 */
    u8 _unknown_0002[0x30d4]; /* 0x02; image data copied from SPR data +0x9200 by 0x80087704 */
    u8 palettes[16][32];      /* 0x30d6; 16 CLUTs with the STP bit set (0x80087704) */
} battle_gfx_spritesheet_slot_t;
typedef char battle_spritesheet_slot_size_must_be_0x32d6[(sizeof(battle_gfx_spritesheet_slot_t) == 0x32d6) ? 1 : -1];

/* Four-byte lookup record for the 0x9f ordinary unit spritesheets. */
typedef struct battle_gfx_spritesheet_data {
    u8 shp_id;
    u8 seq_id;
    u8 flying_flag;
    u8 graphic_height;
} battle_gfx_spritesheet_data_t;
typedef char battle_gfx_spritesheet_data_size_must_be_4[(sizeof(battle_gfx_spritesheet_data_t) == 4) ? 1 : -1];

enum { BATTLE_SPRITESHEET_ID_COUNT = 0x9f };

/* Full-screen fade primitive pair: a TILE followed by the DR_MODE that
 * selects its texture page and window. */
typedef struct battle_gfx_fade_overlay {
    TILE tile;    /* 0x00 */
    DR_MODE mode; /* 0x10 */
} battle_gfx_fade_overlay_t;
typedef char battle_gfx_fade_overlay_size_must_be_0x1c[(sizeof(battle_gfx_fade_overlay_t) == 0x1c) ? 1 : -1];

/* Draw-area pair: two DR_AREA packets followed by their rectangles; the
 * BATTLE twin of world_gfx_scaled_draw_area_pair_t. */
typedef struct battle_gfx_scaled_draw_area_pair {
    DR_AREA areas[2]; /* 0x00 */
    RECT rects[2];    /* 0x18 */
} battle_gfx_scaled_draw_area_pair_t;
typedef char
    battle_gfx_scaled_draw_area_pair_size_must_be_0x28[(sizeof(battle_gfx_scaled_draw_area_pair_t) == 0x28) ? 1 : -1];

/* Sprite part dimensions in 8-pixel tiles, indexed by attribute bits 10-13. */
typedef struct battle_gfx_part_size {
    s32 width;
    s32 height;
} battle_gfx_part_size_t;

/* One of the two 0xee28-byte battle render buffers. The primitive pools are
 * pre-initialised once here; the swap helper at 0x8005a0cc reads the ordering
 * table pointer at +0xe5b4. */
typedef struct battle_render_buffer {
    POLY_GT3 gt3[360]; /* 0x00000 */
    POLY_GT4 gt4[710]; /* 0x03840 */
    POLY_F3 f3[64];    /* 0x0c878 */
    POLY_F4 f4[256];   /* 0x0cd78 */
    POLY_G4 overlay;   /* 0x0e578 */
    DR_MODE overlay_modes[2] /* 0x0e59c */;
    u32* otag;             /* 0x0e5b4 */
    u8 _unused_e5b8[0x70]; /* 0x0e5b8 */
    POLY_FT3 ft3[64];      /* 0x0e628 */
} battle_render_buffer_t;
typedef char battle_render_buffer_size_must_be_0xee28[(sizeof(battle_render_buffer_t) == 0xee28) ? 1 : -1];

/* VRAM load / screen placement point used by battle_gfx_init_image_loading. */
typedef struct battle_image_location {
    s16 x;
    u16 y;
} battle_image_location_t;

extern battle_palette_update_entry_t g_battle_gfx_palette_update_flags[];
extern u16 g_battle_gfx_screen_color_modulation_fade_frames;
extern u16 g_battle_gfx_wait_direction_arrow_palette[16];
extern u16 g_battle_gfx_map_selection_cursor_palette[16];
extern u16 g_battle_gfx_shadow_palette[16];
extern u16 g_battle_gfx_status_bubble_palette[16];
extern s32 g_battle_gfx_status_bubble_delay;
extern u8 g_battle_gfx_screen_color_modulation_target[3];
extern u16 g_battle_gfx_screen_color_modulation_value[3];
extern u16 g_battle_gfx_screen_color_modulation_step[3];
extern RECT g_battle_gfx_screen_modulation_tex_window;

/* Unit graphics loader state: file kind, phase, file or descriptor index and
 * the heap buffer being filled. */
extern s32 g_battle_gfx_state_words[4];
extern u8 g_battle_gfx_cycled_deployed_palettes[2][32];
extern s32 g_battle_gfx_unit_animation_data;
extern u8 g_battle_gfx_item_palettes[0x200];
extern u8 g_battle_gfx_eff_palettes[0x200];
extern u8 g_battle_gfx_frog_palettes[][32];
extern u8 g_battle_gfx_crystal_palettes[][32];
extern u8 g_battle_gfx_treasure_palettes[][32];
extern s32 g_battle_gfx_shp_frame_data_buffer;
extern POLY_F4 g_battle_gfx_screen_modulation_polygons[2];
extern s32 g_battle_gfx_tpage7_free_rect_pixels[4];
extern s32 g_battle_gfx_tpage7_free_rect_cells[4];
extern MATRIX g_battle_gfx_numeric_display_matrix;
extern SVECTOR g_battle_gte_rtps_vxy0;
extern SVECTOR g_battle_gte_rtpt_vxy0;
extern SVECTOR g_battle_gte_rtpt_vxy1;
extern SVECTOR g_battle_gte_rtpt_vxy2;
extern u32 g_battle_otags[2][0x180];
extern battle_file_extent_t g_battle_gfx_sprite_seq_files[];
extern battle_file_extent_t g_battle_gfx_sprite_shp_files[];
extern battle_file_extent_t g_battle_gfx_weapon_seq_files[];
extern battle_file_extent_t g_battle_gfx_weapon_shp_files[];
extern battle_file_extent_t g_battle_gfx_effect_seq_files[];
extern battle_file_extent_t g_battle_gfx_effect_shp_files[];
extern battle_file_extent_t g_battle_gfx_spritesheet_files[];
extern battle_file_extent_t g_battle_gfx_sp2_files[];
extern battle_file_extent_t g_battle_gfx_worker8_sp2_files[];
extern battle_gfx_part_size_t g_battle_gfx_part_sizes[];
extern u8 g_battle_gfx_body_part_seq_data[];
extern u8 g_battle_gfx_body_part_sprite_data[];
extern u8* g_battle_gfx_compressed_data;
extern s32 g_battle_gfx_compressed_nibble_phase;
extern s32 g_battle_gfx_compressed_offset;
extern u8 g_battle_gfx_item_sprite_display_data[];
extern u8 g_battle_gfx_numeric_sprite_display_0[];
extern u8 g_battle_gfx_numeric_sprite_display_1[];
extern u8 g_battle_gfx_numeric_sprite_display_2[];
extern u8 g_battle_gfx_screen_color_modulation[3];
extern u8 g_battle_gfx_screen_color_modulation_backup[3];
extern u8 g_battle_gfx_spritesheet_alt_data[];
extern u8 g_battle_gfx_spritesheet_alt_fallback[];
extern battle_gfx_spritesheet_data_t g_battle_gfx_spritesheet_data[BATTLE_SPRITESHEET_ID_COUNT];
extern battle_gfx_unit_shp_frame_tables_t g_battle_gfx_spritesheet_fallback;
extern u8 g_battle_gfx_spritesheet_ids_by_vram_slot[];
extern battle_gfx_unit_shp_frame_tables_t g_battle_gfx_spritesheet_record_data[5];
extern u8 g_battle_gfx_spritesheet_seq_data[];
extern u8 g_battle_gfx_spritesheet_shp_data[];
extern battle_gfx_spritesheet_slot_t g_battle_gfx_spritesheet_slots[];
extern u8 g_battle_gfx_status_bubble_sprite_display_data[];
extern u8 g_battle_gfx_tpage7_vram_allocation_grid[15][16];
extern u8 g_battle_gfx_unit_sprite_display_data[];
extern battle_gfx_vram_slot_t g_battle_gfx_vram_slots[];
extern u8 g_battle_gfx_weapon_sprite_display_data[];
extern battle_render_buffer_t g_battle_render_buffers[2];

/* The buffer being built this frame; main_gfx_swap_and_clear_otag alternates
 * it between the two g_battle_render_buffers entries. */
extern battle_render_buffer_t* g_battle_data;
extern s32 g_battle_gfx_animation_layer_priorities[][4];

/* Fixed chicken palettes; the other three blocks are g_battle_gfx_frog_palettes,
 * g_battle_gfx_crystal_palettes and g_battle_gfx_treasure_palettes. */
extern u8 g_battle_gfx_chicken_palettes[][32];
extern s16 g_battle_gfx_counter;
extern u8 g_battle_gfx_fallback_seq_data[];
extern u8 g_battle_gfx_fallback_shp_frame_data[];
extern u8 g_battle_gfx_item_graphic_data[];
extern s32 g_battle_gfx_last_loaded_seq_id;
extern s32 g_battle_gfx_last_loaded_shp_id;

/* Byte-granular bump-allocation cursors: the consuming files do raw byte
 * arithmetic on these, so the element type must stay u8. */
extern u8* g_battle_gfx_load_data_cursor;
extern u8* g_battle_gfx_shp_frame_data_cursor;
extern s16 g_battle_gfx_previous_counter;
extern battle_gfx_fade_overlay_t g_battle_gfx_screen_fade_overlays[]; /* one fade overlay per packet buffer */

/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_battle_gfx_screen_modulation_draw_modes[2];

/* Frame counter and frame index of the selection cursor's bob animation. */
extern s32 g_battle_gfx_selection_cursor_bob_timer;
extern s32 g_battle_gfx_selection_cursor_bob_frame;
extern void* g_battle_gfx_sp2_data;
extern u16 g_battle_gfx_sprite_y_rotations[];

/* Status flag shown by each status bubble id (0x16 words at 0x80093cb4).
 * Bubbles 0 (KO), 0x14 (Death Sentence) and 0x15 (casting) hold 0 and are
 * tested separately by battle_gfx_determine_status_bubble_parameters. */
extern s32 g_battle_gfx_status_bubble_status_masks[];
extern u8 g_battle_gfx_targeted_frame_back[];
extern u8 g_battle_gfx_targeted_frame_front[];

/* Advanced by g_animation_speed once per call and sampled with bit 5 to blink
 * the highlighted arrow. */
extern u32 g_battle_gfx_wait_arrow_blink_timer;

/* Separate triggers, command arguments, and completion status. Argument
 * domains remain provisional: map commands 0x80/0x83, polls 0x81/0x84.
 * The u16 requests and arguments are read with lhu by the consumer
 * battle_script_process_pending_requests; every other access is a store. */
extern u16 g_battle_3d_object_use_request; /* 0x80165fe2 */
extern u16 g_battle_3d_object_wait_status; /* 0x8016606e */
extern battle_render_buffer_t g_battle_buffer_a[2];
void battle_display_menu_number_entry(s32 value, u8 digit_count, s32 image, u16* origin);
void battle_gfx_draw_status_bubble(battle_unit_misc_data_t* unit, const u16* screen_coords);
void battle_gfx_run_unit_seq_script(battle_unit_misc_data_t* unit, battle_unit_anim_state_t* state, s32 advance);

void battle_gfx_run_wep_eff_seq_script(
    battle_unit_misc_data_t* unit, battle_unit_sprite_block_t* sprite, s32 index, s32 advance);

void battle_gfx_build_cursor_tile_glow(
    s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot);

void battle_gfx_draw_map_selection_cursor(SVECTOR* base, u32* ot);
void battle_gfx_draw_wait_direction_arrows(SVECTOR* vector, u32* ot);
s32 battle_gfx_build_next_action_result_display(battle_unit_misc_data_t* unit);
void battle_gfx_determine_status_bubble_parameters(battle_unit_misc_data_t*);
s32 battle_gfx_claim_spritesheet_slot(s16 spritesheet_id);
void battle_gfx_update_and_animate_unit_wep_eff(battle_unit_misc_data_t* unit);
void battle_gfx_draw_unit_sprite_layers(battle_unit_misc_data_t* unit);
void battle_gfx_init_scanline_frame(void);
void battle_gfx_prepare_post_action_display(battle_unit_misc_data_t* unit);
void battle_gfx_set_thrown_item_graphic_palette(s32 graphic_id, battle_unit_misc_data_t* unit);
void battle_gfx_update_all_unit_rotation_and_vectors(void);
void battle_gfx_tint_unit_palette_for_weather_and_tile(battle_unit_misc_data_t* unit, s32 level, s32 final_value);
void battle_gfx_animate_post_action_text(battle_unit_misc_data_t* unit, const u16* screen_coords);
void battle_gfx_apply_status_spritesheet_change(battle_unit_misc_data_t* unit, s32 tile_effect_level);
s32 battle_gfx_build_next_special_action_result_display(battle_unit_misc_data_t* unit);
void battle_gfx_calculate_sprite_shadow_from_tile_slope(battle_unit_misc_data_t* unit);
void battle_gfx_draw_unit_shadow(battle_unit_misc_data_t* unit, u32* ot);
void battle_gfx_draw_unit_sprite_position_copies(battle_unit_misc_data_t* unit);
u32 battle_gfx_get_unit_spritesheet_height(battle_unit_misc_data_t* unit);
void battle_gfx_init_item_get_rendering(battle_gfx_render_unit_t* unit);
battle_gfx_sprite_display_data_t* battle_gfx_init_item_sprite_display(s32 unit_index);

battle_gfx_sprite_display_data_t* battle_gfx_init_numeric_sprite_display(
    s32 unit_index, s32 display_section, battle_gfx_sprite_display_data_t* default_display);

void battle_gfx_init_status_bubble(battle_unit_misc_data_t* unit);
battle_gfx_sprite_display_data_t* battle_gfx_init_status_bubble_sprite_display(s32 unit_index);
battle_gfx_sprite_display_data_t* battle_gfx_init_weapon_sprite_display(s32 unit_index, s32 display_type);

void battle_gfx_load_trap_and_unit_frame_parts(
    battle_unit_misc_data_t* unit, battle_unit_anim_state_t* sprite, u16 frame, u16 animation);

void battle_gfx_load_unit_frame_parts(battle_unit_misc_data_t* unit, battle_gfx_sprite_display_data_t* display,
    battle_gfx_source_frame_t* src, s32 sheet_flags);

void battle_gfx_load_unit_into_evtchr_slot(battle_unit_misc_data_t* unit, s32 slot, s32 image_slot);

void battle_gfx_modify_misc_unit_palette(battle_unit_misc_data_t* misc, s32 mode, s32 section_id, s32 unit_misc_id,
    s32 preset, s16 red, s16 green, s16 blue, s32 final_value);

s32 battle_gfx_open_sp2_file(battle_unit_misc_data_t* unit, void* destination);

void battle_gfx_select_unit_shp_frame(battle_unit_misc_data_t* unit, battle_unit_anim_state_t* sprite, u16 frame,
    u16 animation, s32** out_table, s32* out_value);

void battle_gfx_set_sprite_part_flag_bit_0(battle_gfx_sprite_display_data_t* display, s32 part_index, s32 enabled);
void battle_gfx_shift_and_fill_display_svectors(battle_gfx_render_unit_t* unit);
void battle_gfx_store_item_display_data(battle_gfx_render_unit_t* unit, u32 item_id);
void battle_gfx_unpack_unit_shp_data(battle_gfx_unit_shp_frame_tables_t* dst, u8* src);
void battle_gfx_update_misc_unit_status_palette(battle_unit_misc_data_t* misc, s32 tile_effect_level, s32 final_value);
s32 battle_gfx_prepare_post_action_display_by_misc_id(u16 misc_id);
s32 battle_gfx_calculate_screen_z_from_misc_battle_map_data(struct battle_unit_misc_data* unit);
s32 battle_gfx_calculate_screen_z_from_misc_move_data(struct battle_unit_misc_data* unit);

/* Returns (s16), but an s16 prototype makes callers such as
 * battle_unit_update_distortion_height re-extend the result; keep s32. */
s32 battle_gfx_calculate_screen_z_from_misc_screen_data(struct battle_unit_misc_data* unit);

s32 battle_gfx_calculate_screen_z_with_caller_data(
    struct battle_unit_misc_data* unit, const battle_screen_coords_t* coords);

void battle_gfx_init_render_buffers(void);
void battle_gfx_append_gpu_primitive_to_secondary_otag(u32* primitive);

void battle_gfx_append_unit_graphics_load_descriptor(u8 map_x, u8 map_y, u8 map_level, u16 map_height, s32 portrait_id,
    u16 palette_id, u16 misc_id, struct battle_stats* g_battle_unit_stats, u32 flags);

s32 battle_gfx_calculate_screen_z_from_misc_map_data(struct battle_unit_misc_data* unit);

void battle_gfx_check_tile_status_palette_mod(
    struct battle_unit_misc_data* unit, s32 tile_effect_level, s32 final_value);

void battle_gfx_clear_four_state_words(void);
s32 battle_gfx_clear_shadow_graphic_trigger_by_misc_id(u32 misc_id);
void battle_gfx_clear_tpage7_vram_allocation_grid(void);
void battle_gfx_configure_misc_unit_palette_modulation_1f(s32 unit_id);

void battle_gfx_construct_polygon_data_for_units(
    battle_gfx_sprite_display_t* display, s32 end, s32 start, s16* position, s16 angle, u16 mode, s16* scale, u32* ot);

void battle_gfx_decompress_attack_spritesheet(u8* source, u8* destination);
void battle_gfx_draw_or_append_gpu_primitive(void* primitive);
void battle_gfx_draw_screen_color_modulation_overlay(void);
void battle_gfx_draw_wait_direction_unit(void);
void battle_gfx_free_tpage7_vram(RECT* rect);
u32 battle_gfx_get_spritesheet_flying_flag(u32 spritesheet_id);
u8* battle_gfx_get_spritesheet_seq_data_address(u32 index);
battle_gfx_unit_shp_frame_tables_t* battle_gfx_get_spritesheet_shp_data_address(u32 index);
s32 battle_gfx_get_spritesheet_vram_by_battle_id(u32 battle_id);
void battle_gfx_tint_all_units_by_team(void);
void battle_gfx_init_default_poly_ft4(POLY_FT4* primitive);
void battle_gfx_init_earned_exp_jp_display(struct battle_unit_misc_data* unit);
s32 battle_gfx_init_evtchr_vram_slots(void);
void battle_gfx_init_misc_unit_palette_modulation(s32 misc_id, s32 mode, s16 red, s16 green, s16 blue);
void battle_gfx_init_position_vector_copies(struct battle_unit_misc_data* unit);
void battle_gfx_init_render_frame(void);
void battle_gfx_init_render_state(void);
void battle_gfx_load_casting_unit_graphics(void);
void battle_gfx_load_spritesheet_into_vram_slot(u8* data, u32 index, s32 unused, struct battle_unit_misc_data* unit);
s32 battle_gfx_load_unit_graphics_by_battle_id(s32, s32);
void battle_gfx_modify_palette_based_on_team(struct battle_unit_misc_data* unit, s32 amount);
u32 battle_gfx_read_compressed_nibble(void);
void battle_gfx_reset_jumping_unit_graphic_triggers(void);
s32 battle_gfx_reset_unit_graphic_trigger(u32 misc_id);
void battle_gfx_set_evtchr_palette_by_misc_id(s32 palette_row_index, s32 slot_index, s32 clut_index);
s32 battle_gfx_set_shadow_graphic_trigger_by_misc_id(u32 misc_id);
void battle_gfx_set_thrown_item_palette_by_misc_id(u32 item_id, u32 misc_id);
void battle_gfx_submit_screen_fade_overlay(battle_gfx_fade_overlay_t* overlay, s32 level);
void battle_gfx_update_screen_color_modulation_fade(void);
void battle_gfx_update_sprite_transparency_flag(struct battle_unit_misc_data* unit);
void battle_gfx_update_status_bubble_display(struct battle_unit_misc_data* unit);
void battle_gfx_update_status_bubble_graphic_trigger(struct battle_unit_misc_data* unit);
void battle_gfx_update_status_bubbles_and_graphics(void);
void battle_gfx_update_unit_palette_animation(void);
void battle_gfx_update_unit_palettes(void);
void battle_gfx_alloc_tpage7_vram(RECT* rect, union battle_texture_prim* prim, u32* image);
u8* battle_gfx_get_unit_sprite_frame_and_vram_ids(u32 misc_id, battle_unit_sprite_query_t* out);
void battle_gfx_apply_weather_time_tile_palette_mod_by_misc_id(u32 misc_id);
s32 battle_gfx_clear_loaded_evtchr_slot(s32 slot_index);
s32 battle_gfx_clear_saved_evtchr_slot(s32 slot_index);
void battle_gfx_configure_misc_unit_palette_modulation(s32 misc_id, s32 mode, s16 red, s16 green, s16 blue);
void battle_gfx_extract_deployed_unit_palettes(void);
void battle_gfx_flip_poly_ft4_uv_if_negative(POLY_FT4* primitive, s32 direction);
u8* battle_gfx_get_body_part_seq_data(void);
u8* battle_gfx_get_body_part_sprite_data(void);
u8* battle_gfx_get_spritesheet_seq_data(u32 spritesheet_id);
u8* battle_gfx_get_spritesheet_shp_data(u32 spritesheet_id);
u32 battle_gfx_get_unit_spritesheet_height_by_misc_id(u32 misc_id);
s32 battle_gfx_get_unit_spritesheet_height_by_unit_id(u32 unit_id);
void battle_gfx_highlight_all_units_blue_or_red(s32 mode);
s16 battle_gfx_increment_counter(void);
void battle_gfx_init_deployment_and_reset_unit_graphics_state(void);
u8* battle_gfx_init_unit_sprite_display(s32 unit_index);
s32 battle_gfx_is_spritesheet_id_loaded(s32 spritesheet_id);
s32 battle_gfx_load_misc_unit_into_evtchr_slot(s32 misc_id, s32 slot);

void battle_gfx_modify_vram_palette(s32 mode, const u16* source, s32 section_id, s32 unit_misc_id, s32 preset, s16 red,
    s16 green, s16 blue, s32 final_value);

s32 battle_gfx_release_reserved_vram_slot(s32 slot_index);
void battle_gfx_release_unit_vram_slot(struct battle_unit_misc_data* unit);
void battle_gfx_reset_unit_graphics_load_state(void);
s32 battle_gfx_save_evtchr_slot(s32 slot_index);
void battle_gfx_set_draw_mode_for_texture_page(DR_MODE* mode, s32 page);
void battle_gfx_set_draw_mode_from_rect(void* mode, const u16* rect);
void battle_gfx_set_primitive_brightness(u8* primitive, s32 brightness);
void battle_gfx_start_screen_color_modulation_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
s32 battle_gfx_step_load_unit_shp_seq_data(void);

void battle_gfx_store_sprite_display_data(battle_gfx_sprite_display_data_t* display, s32 index, s8 x_shift, s8 y_shift,
    u16 u, u16 v, u16 width, u16 height, u16 flags);

s32 battle_gfx_unpack_evtchr_file_to_vram_slot(s32 index, u8* file);

void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

void battle_gfx_copy_other_spr_palette_data_to_ram(u8* src);
void battle_gfx_load_wep_spr_data(u8* data);

void battle_gfx_apply_misc_unit_palette_modulation(
    s32 preset_color, s32 time, s32 misc_id, s32 red, s32 green, s32 blue);

void battle_gfx_start_misc_unit_palette_modulation(
    s32 preset_color, s32 time, s32 misc_id, s32 red, s32 green, s32 blue);

void battle_gfx_load_evtchr_thread(void);

/* 0x8008d104..0x8008d138: drains immediate loader steps (2), returns
 * 0 when finished or 1 while work remains pending (including allocation retry). */
s32 battle_gfx_poll_unit_graphics_load(void);

/* 0x80088904: 0 finished, 1 pending/deferred, 2 immediate progress. */
s32 battle_gfx_step_queued_unit_graphics_load(void);

/* action */
typedef enum battle_action_type {
    BATTLE_ACTION_TYPE_PSEUDO_STATUS = 0x01,
    BATTLE_ACTION_TYPE_STATUS_CHANGE = 0x08,
    BATTLE_ACTION_TYPE_MP_HEALING = 0x10,
    BATTLE_ACTION_TYPE_MP_DAMAGE = 0x20,
    BATTLE_ACTION_TYPE_HP_HEALING = 0x40,
    BATTLE_ACTION_TYPE_HP_DAMAGE = 0x80,
} battle_action_type_e;

typedef enum battle_action_miss_type {
    BATTLE_ACTION_MISS_TYPE_HIT = 0x00,
    BATTLE_ACTION_MISS_TYPE_ACCESSORY_EVADE = 0x01,
    BATTLE_ACTION_MISS_TYPE_RIGHT_HAND_EVADE = 0x02,
    BATTLE_ACTION_MISS_TYPE_LEFT_HAND_EVADE = 0x03,
    BATTLE_ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD = 0x04,
    BATTLE_ACTION_MISS_TYPE_NULLIFIED = 0x05,
    BATTLE_ACTION_MISS_TYPE_ACCURACY_MISS = 0x06,
    BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE = 0x07,
    BATTLE_ACTION_MISS_TYPE_CANCELLED = 0x08,
    BATTLE_ACTION_MISS_TYPE_REFLECTED = 0x09,
    BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD = 0x0a,
    BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD = 0x0b,
    BATTLE_ACTION_MISS_TYPE_BUGGED_REFLECTION = 0x0c,
    BATTLE_ACTION_MISS_TYPE_CATCH = 0x0d,
} battle_action_miss_type_e;

typedef enum battle_action_special_effect {
    BATTLE_ACTION_SPECIAL_EFFECT_SET_GOLEM = 0x0001,
    BATTLE_ACTION_SPECIAL_EFFECT_MORBOL = 0x0002,
    BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT = 0x0004,
    BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN = 0x0008,
    BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM = 0x0010,
    BATTLE_ACTION_SPECIAL_EFFECT_POACH = 0x0020,
    BATTLE_ACTION_SPECIAL_EFFECT_TEAM_CHANGE = 0x0040,
    BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_UP = 0x0080,
    BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_DOWN = 0x0100,
    BATTLE_ACTION_SPECIAL_EFFECT_NULLIFIED = 0x0200,
    BATTLE_ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION = 0x0400,
    BATTLE_ACTION_SPECIAL_EFFECT_ELEMENTAL_WEAKNESS = 0x0800,
    BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN = 0x1000,
    BATTLE_ACTION_SPECIAL_EFFECT_KNOCKBACK = 0x4000,
    BATTLE_ACTION_SPECIAL_EFFECT_GOLEM_GUARD = 0x8000,
} battle_action_special_effect_e;

typedef enum battle_action_context {
    BATTLE_ACTION_CONTEXT_PRIMARY = 0,
    BATTLE_ACTION_CONTEXT_REACTION_OR_SIMULATION = 1,
} battle_action_context_e;

typedef enum battle_action_state {
    BATTLE_ACTION_STATE_EXECUTE = 0,
    BATTLE_ACTION_STATE_AI_SIMULATION = 1,
    BATTLE_ACTION_STATE_PREVIEW = 2,
} battle_action_state_e;

typedef enum battle_action_target_kind {
    BATTLE_ACTION_TARGET_TILE = 5,
    BATTLE_ACTION_TARGET_UNIT = 6,
} battle_action_target_kind_e;

/* Pending post-action displays are queued as one little-endian word in Misc
 * Unit Data 0x1b8..0x1bb.  The two dispatchers at 0x8007f5f8 and 0x800808b8
 * consume one bit at a time in ascending order. */
typedef enum battle_action_display_flag {
    BATTLE_ACTION_DISPLAY_FLAG_HP_DAMAGE = 0x00000001,
    BATTLE_ACTION_DISPLAY_FLAG_HP_HEALING = 0x00000002,
    BATTLE_ACTION_DISPLAY_FLAG_MP_DAMAGE = 0x00000004,
    BATTLE_ACTION_DISPLAY_FLAG_MP_HEALING = 0x00000008,
    BATTLE_ACTION_DISPLAY_FLAG_SPEED_DAMAGE = 0x00000010,
    BATTLE_ACTION_DISPLAY_FLAG_SPEED_GAIN = 0x00000020,
    BATTLE_ACTION_DISPLAY_FLAG_CT_DAMAGE = 0x00000040,
    BATTLE_ACTION_DISPLAY_FLAG_CT_GAIN = 0x00000080,
    BATTLE_ACTION_DISPLAY_FLAG_BRAVE_DAMAGE = 0x00000100,
    BATTLE_ACTION_DISPLAY_FLAG_BRAVE_GAIN = 0x00000200,
    BATTLE_ACTION_DISPLAY_FLAG_FAITH_DAMAGE = 0x00000400,
    BATTLE_ACTION_DISPLAY_FLAG_FAITH_GAIN = 0x00000800,
    BATTLE_ACTION_DISPLAY_FLAG_PA_DAMAGE = 0x00001000,
    BATTLE_ACTION_DISPLAY_FLAG_PA_GAIN = 0x00002000,
    BATTLE_ACTION_DISPLAY_FLAG_MA_DAMAGE = 0x00004000,
    BATTLE_ACTION_DISPLAY_FLAG_MA_GAIN = 0x00008000,
    BATTLE_ACTION_DISPLAY_FLAG_LEVEL_DOWN = 0x00010000,
    BATTLE_ACTION_DISPLAY_FLAG_LEVEL_UP = 0x00020000,
    BATTLE_ACTION_DISPLAY_FLAG_STOLEN = 0x00040000,
    BATTLE_ACTION_DISPLAY_FLAG_BROKEN = 0x00080000,
    BATTLE_ACTION_DISPLAY_FLAG_QUICK = 0x00100000,
    BATTLE_ACTION_DISPLAY_FLAG_CT_ZERO = 0x00200000,
    BATTLE_ACTION_DISPLAY_FLAG_MISSED = 0x00400000,
    BATTLE_ACTION_DISPLAY_FLAG_GUARDED = 0x00800000,
    BATTLE_ACTION_DISPLAY_FLAG_CAUGHT = 0x01000000,
    BATTLE_ACTION_DISPLAY_FLAG_GAINED_EXP = 0x02000000,
    BATTLE_ACTION_DISPLAY_FLAG_GAINED_JP = 0x04000000,
    BATTLE_ACTION_DISPLAY_FLAG_NO_TARGET = 0x08000000,
    BATTLE_ACTION_DISPLAY_FLAG_SILENCED = 0x10000000,
    BATTLE_ACTION_DISPLAY_FLAG_NO_MP = 0x20000000,
} battle_action_display_flag_e;

/* Image IDs used by the projected-action display. These index the 49-entry
 * texture-source table at 0x8014cf68; they are not battle status IDs. */
typedef enum battle_action_display_image_id {
    BATTLE_ACTION_DISPLAY_IMAGE_DEAD = 0x00,
    BATTLE_ACTION_DISPLAY_IMAGE_UNDEAD = 0x01,
    BATTLE_ACTION_DISPLAY_IMAGE_PETRIFY = 0x02,
    BATTLE_ACTION_DISPLAY_IMAGE_INVITE = 0x03,
    BATTLE_ACTION_DISPLAY_IMAGE_DARKNESS = 0x04,
    BATTLE_ACTION_DISPLAY_IMAGE_CONFUSION = 0x05,
    BATTLE_ACTION_DISPLAY_IMAGE_SILENCE = 0x06,
    BATTLE_ACTION_DISPLAY_IMAGE_BLOOD_SUCK = 0x07,
    BATTLE_ACTION_DISPLAY_IMAGE_OIL = 0x08,
    BATTLE_ACTION_DISPLAY_IMAGE_FLOAT = 0x09,
    BATTLE_ACTION_DISPLAY_IMAGE_RERAISE = 0x0a,
    BATTLE_ACTION_DISPLAY_IMAGE_TRANSPARENT = 0x0b,
    BATTLE_ACTION_DISPLAY_IMAGE_BERSERK = 0x0c,
    BATTLE_ACTION_DISPLAY_IMAGE_POISON = 0x0d,
    BATTLE_ACTION_DISPLAY_IMAGE_REGEN = 0x0e,
    BATTLE_ACTION_DISPLAY_IMAGE_PROTECT = 0x0f,
    BATTLE_ACTION_DISPLAY_IMAGE_SHELL = 0x10,
    BATTLE_ACTION_DISPLAY_IMAGE_HASTE = 0x11,
    BATTLE_ACTION_DISPLAY_IMAGE_SLOW = 0x12,
    BATTLE_ACTION_DISPLAY_IMAGE_STOP = 0x13,
    BATTLE_ACTION_DISPLAY_IMAGE_FAITH = 0x14,
    BATTLE_ACTION_DISPLAY_IMAGE_INNOCENT = 0x15,
    BATTLE_ACTION_DISPLAY_IMAGE_CHARM = 0x16,
    BATTLE_ACTION_DISPLAY_IMAGE_SLEEP = 0x17,
    BATTLE_ACTION_DISPLAY_IMAGE_DONT_MOVE = 0x18,
    BATTLE_ACTION_DISPLAY_IMAGE_DONT_ACT = 0x19,
    BATTLE_ACTION_DISPLAY_IMAGE_REFLECT = 0x1a,
    BATTLE_ACTION_DISPLAY_IMAGE_DEATH_SENTENCE = 0x1b,
    BATTLE_ACTION_DISPLAY_IMAGE_STOLEN = 0x1c,
    BATTLE_ACTION_DISPLAY_IMAGE_BROKEN = 0x1d,
    BATTLE_ACTION_DISPLAY_IMAGE_QUICK = 0x24,
    BATTLE_ACTION_DISPLAY_IMAGE_HP = 0x25,
    BATTLE_ACTION_DISPLAY_IMAGE_MP = 0x26,
    BATTLE_ACTION_DISPLAY_IMAGE_CT = 0x27,
    BATTLE_ACTION_DISPLAY_IMAGE_SPEED = 0x28,
    BATTLE_ACTION_DISPLAY_IMAGE_BRAVE = 0x29,
    BATTLE_ACTION_DISPLAY_IMAGE_FAITH_STAT = 0x2a,
    BATTLE_ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK = 0x2b,
    BATTLE_ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK = 0x2c,
    BATTLE_ACTION_DISPLAY_IMAGE_LEVEL = 0x2d,
    BATTLE_ACTION_DISPLAY_IMAGE_GIL = 0x2e,
    BATTLE_ACTION_DISPLAY_IMAGE_EXP = 0x2f,
    BATTLE_ACTION_DISPLAY_IMAGE_FROG = 0x30,
    BATTLE_ACTION_DISPLAY_IMAGE_NONE = 0xff,
} battle_action_display_image_id_e;

enum { BATTLE_ACTION_DISPLAY_IMAGE_COUNT = BATTLE_ACTION_DISPLAY_IMAGE_FROG + 1 };

typedef struct battle_action_display_image_rect {
    u8 u;
    u8 v;
    u8 width;
    u8 height;
} battle_action_display_image_rect_t;
typedef char
    battle_action_display_image_rect_size_must_be_4[(sizeof(battle_action_display_image_rect_t) == 4) ? 1 : -1];

typedef enum element_flags {
    BATTLE_ELEMENT_FIRE = 0x80,
    BATTLE_ELEMENT_LIGHTNING = 0x40,
    BATTLE_ELEMENT_ICE = 0x20,
    BATTLE_ELEMENT_WIND = 0x10,
    BATTLE_ELEMENT_EARTH = 0x08,
    BATTLE_ELEMENT_WATER = 0x04,
    BATTLE_ELEMENT_HOLY = 0x02,
    BATTLE_ELEMENT_DARK = 0x01,
} element_flags_e;

/* Shared ordering for job, item-attribute, and runtime unit affinity arrays.
 * Job records contain only the first four entries; item and runtime records
 * also contain Strengthen. */
typedef enum elemental_affinity_index {
    ELEMENTAL_AFFINITY_ABSORB = 0,
    ELEMENTAL_AFFINITY_NULLIFY = 1,
    ELEMENTAL_AFFINITY_HALF = 2,
    ELEMENTAL_AFFINITY_WEAK = 3,
    ELEMENTAL_AFFINITY_STRENGTHEN = 4,
    ELEMENTAL_AFFINITY_COUNT = 5,
} elemental_affinity_index_e;

/* Shared command prefix at battle_stats_t +0x16e and AI action +0x08.
 * The charge evaluator at 0x80195f8c reads ability_id as unsigned; simulation
 * at 0x8019dd8c reads the same bits as signed. This is not the full command. */
typedef struct battle_action_command_prefix {
    u8 actor_id;
    u8 skillset_id;
    u16 ability_id;
} battle_action_command_prefix_t;
typedef char battle_action_command_prefix_size_must_be_4[(sizeof(battle_action_command_prefix_t) == 4) ? 1 : -1];

/* Ordinary SP/PA/MA/Brave/Faith action-result encoding. CT and EXP reuse
 * these bits with distinct sentinel or sign meanings and are excluded. */
typedef enum battle_action_stat_change {
    BATTLE_ACTION_STAT_CHANGE_VALUE_MASK = 0x7f,
    BATTLE_ACTION_STAT_CHANGE_INCREASE = 0x80,
} battle_action_stat_change_e;

/* Bit order mirrors the seven serialized battle_stats_t equipment slots. */
typedef enum battle_action_equipment_slot_mask {
    BATTLE_ACTION_EQUIPMENT_SLOT_HEAD = 0x80,
    BATTLE_ACTION_EQUIPMENT_SLOT_BODY = 0x40,
    BATTLE_ACTION_EQUIPMENT_SLOT_ACCESSORY = 0x20,
    BATTLE_ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON = 0x10,
    BATTLE_ACTION_EQUIPMENT_SLOT_RIGHT_SHIELD = 0x08,
    BATTLE_ACTION_EQUIPMENT_SLOT_LEFT_WEAPON = 0x04,
    BATTLE_ACTION_EQUIPMENT_SLOT_LEFT_SHIELD = 0x02,
} battle_action_equipment_slot_mask_e;

/* The strike loader fills this 40-byte work record; it is distinct from
 * battle_stats_t.action. Reaction targeting writes redirected tile fields at
 * +0x1c..+0x1e before the strike is processed. */
typedef struct battle_strike_work {
    u8 actor_id;
    u8 target_count;
    u8 target_list[16];
    u8 animate_on_miss_flag;
    u8 control_value_19f;
    s16 last_attack_id;
    u8 ability_formula;
    u8 reaction_occurred;
    u8 continue_attack;
    u8 current_hit_number;
    u16 reaction_id_1a;
    u8 target_new_x;
    u8 target_new_y;
    u8 target_new_map_level;
    u8 used_weapon_id;
    u16 reaction_ability_id; /* 0x20; misc 0x1ac */
    /* 0x22; misc 0x1ae. g_current_ability.knockback_flags & 0x7f, stored with
     * the knockback destination. */
    u8 knockback_flags;
    u8 can_earn_experience;
    u8 _unused_24[4];
} battle_strike_work_t;
typedef char battle_strike_work_must_be_40[sizeof(battle_strike_work_t) == 40 ? 1 : -1];
typedef char battle_strike_targets_must_be_2[((unsigned long)&((battle_strike_work_t*)0)->target_list == 2) ? 1 : -1];
typedef char
    battle_strike_continue_must_be_0x18[((unsigned long)&((battle_strike_work_t*)0)->continue_attack == 0x18) ? 1 : -1];

/* Block view of the numeric display fields at misc unit data 0x2bc..0x2cf,
 * taken as one pointer by the action result display builders
 * (battle_gfx_build_next_action_result_display, 0x800808b8, battle_gfx_animate_post_action_text). */
typedef struct battle_action_result_display {
    u8 active;                                     /* 0x00 */
    u8 _padding_01;                                /* 0x01; aligns selector */
    u16 selector;                                  /* 0x02; battle_numeric_display_selector_e */
    u16 value;                                     /* 0x04 */
    s16 progress;                                  /* 0x06 */
    battle_gfx_sprite_display_data_t* displays[3]; /* 0x08 */
} battle_action_result_display_t;
typedef char battle_action_result_display_size_must_be_0x14[(sizeof(battle_action_result_display_t) == 0x14) ? 1 : -1];

/* Five-byte range/targeting work entry at 0x80192dd8, 256 entries per level.
 * Movement-cost propagation uses remaining_range as the residual budget;
 * the AI reads it at 0x801995f4 and 0x801996ac. */
typedef struct battle_target_panel {
    u8 remaining_range;
    /* Targeting frontier/targeted mark; pathing stores the step number here. */
    u8 mark;
    /* Index 0..15 of the compact movement record on the tile (0x80174e84),
     * not a battle id. */
    u8 unit_record_index;
    /* Remaining range after landing on a rideable unit (0x80175fa4); cleared on
     * the acting tile by 0x80178dac; pathing reuses it on 512+ panels as the step number. */
    u8 ride_remaining_range;
    /* Largest single height step along the path, the equal-budget tie-breaker
     * (0x80175958, 0x80177794); 0xff initial value in mode 1. AI propagation
     * sets the target to 0 and old actor tile to 0xff. */
    u8 max_height_delta;
} battle_target_panel_t;
typedef char battle_targeting_panel_size_must_be_5[(sizeof(battle_target_panel_t) == 5) ? 1 : -1];

/* One five-byte targeting panel entry, 0x200 of them, indexed in parallel with
 * the map tile data. Nine routines walk the table with `+= 5` byte arithmetic. */
typedef struct targeting_panel_entry {
    s8 a;
    s8 b;
    s8 c[3];
} targeting_panel_entry_t;
typedef char targeting_panel_entry_size_must_be_5[(sizeof(targeting_panel_entry_t) == 5) ? 1 : -1];

/* Current ability data at 0x801938c0: the attacker/target pair, weapons, formula
 * inputs and results of the strike being resolved, with copies of the ability's
 * secondary data, the weapon's data and the status-infliction record that
 * battle_action_run_pre_formula_setup loads. */
typedef struct battle_current_ability {
    u8 attacker_id;                             /* 0x00 */
    u8 target_id;                               /* 0x01 */
    u8 strike_count;                            /* 0x02 */
    u8 strike_counter;                          /* 0x03: weapon hand, 0 = right, 1 = left */
    u8 primary_weapon_id;                       /* 0x04 */
    u8 secondary_weapon_id;                     /* 0x05 */
    u16 reaction_id;                            /* 0x06 */
    u16 elemental_flags;                        /* 0x08 */
    u8 target_x;                                /* 0x0a */
    u8 target_y;                                /* 0x0b */
    u8 target_elevation;                        /* 0x0c */
    u8 terrain;                                 /* 0x0d: surface type under the target */
    u16 xa;                                     /* 0x0e: formula steps that divide it signed read it as s16 */
    u16 ya;                                     /* 0x10: likewise */
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
    u8 right_shield_evade;                      /* 0x1f */
    u8 left_shield_evade;                       /* 0x20 */
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
    u8 post_formula_flag;                       /* 0x4e */
    u8 random_fire_flag;                        /* 0x4f */
} battle_current_ability_t;
typedef char battle_current_ability_size_must_be_0x50[(sizeof(battle_current_ability_t) == 0x50) ? 1 : -1];

/* Current attacker record at 0x8019389c, filled by
 * battle_action_set_current_attacker_data and read by
 * battle_action_set_mimic_ability. The AI saves and restores it whole as 30
 * bytes (battle_ai_store_considered_action_data). */
typedef struct battle_current_attacker_data {
    u8 facing;                         /* 0x00 */
    u8 data_initialized;               /* 0x01: set once the fields below are filled; no stronger meaning yet */
    u8 right_hand_weapon_id;           /* 0x02 */
    u8 left_hand_weapon_id;            /* 0x03 */
    u8 team;                           /* 0x04 */
    u8 _padding_05;                    /* 0x05; aligns target_delta_x */
    s16 target_delta_x;                /* 0x06 */
    s16 target_delta_y;                /* 0x08 */
    battle_ai_command_action_t action; /* 0x0a: copy of battle_stats_t +0x16e */
} battle_current_attacker_data_t;
typedef char battle_current_attacker_data_size_must_be_30[(sizeof(battle_current_attacker_data_t) == 30) ? 1 : -1];

/* Active reaction abilities occupy four MSB-first bytes at
 * battle_stats_t + 0x08b. These values are verified against the target's
 * reaction dispatchers. */
typedef enum battle_unit_reaction_set_1 {
    BATTLE_REACTION_SET_1_PA_SAVE = 0x80,
    BATTLE_REACTION_SET_1_MA_SAVE = 0x40,
    BATTLE_REACTION_SET_1_SPEED_SAVE = 0x20,
    BATTLE_REACTION_SET_1_SUNKEN_STATE = 0x10,
    BATTLE_REACTION_SET_1_CAUTION = 0x08,
    BATTLE_REACTION_SET_1_DRAGON_SPIRIT = 0x04,
    BATTLE_REACTION_SET_1_REGENERATOR = 0x02,
    BATTLE_REACTION_SET_1_BRAVE_UP = 0x01,
} battle_unit_reaction_set_1_e;

typedef enum battle_unit_reaction_set_2 {
    BATTLE_REACTION_SET_2_FAITH_UP = 0x80,
    BATTLE_REACTION_SET_2_HP_RESTORE = 0x40,
    BATTLE_REACTION_SET_2_MP_RESTORE = 0x20,
    BATTLE_REACTION_SET_2_CRITICAL_QUICK = 0x10,
    BATTLE_REACTION_SET_2_MEATBONE_SLASH = 0x08,
    BATTLE_REACTION_SET_2_COUNTER_MAGIC = 0x04,
    BATTLE_REACTION_SET_2_COUNTER_TACKLE = 0x02,
    BATTLE_REACTION_SET_2_COUNTER_FLOOD = 0x01,
} battle_unit_reaction_set_2_e;

typedef enum battle_unit_reaction_set_3 {
    BATTLE_REACTION_SET_3_ABSORB_USED_MP = 0x80,
    BATTLE_REACTION_SET_3_GILGAME_HEART = 0x40,
    BATTLE_REACTION_SET_3_REFLECT = 0x20,
    BATTLE_REACTION_SET_3_AUTO_POTION = 0x10,
    BATTLE_REACTION_SET_3_COUNTER = 0x08,
    /* 0x04 is present in the bitset but remains unidentified. */
    BATTLE_REACTION_SET_3_DISTRIBUTE = 0x02,
    BATTLE_REACTION_SET_3_MP_SWITCH = 0x01,
} battle_unit_reaction_set_3_e;

typedef enum battle_unit_reaction_set_4 {
    BATTLE_REACTION_SET_4_DAMAGE_SPLIT = 0x80,
    BATTLE_REACTION_SET_4_WEAPON_GUARD = 0x40,
    BATTLE_REACTION_SET_4_FINGER_GUARD = 0x20,
    BATTLE_REACTION_SET_4_ABANDON = 0x10,
    BATTLE_REACTION_SET_4_CATCH = 0x08,
    BATTLE_REACTION_SET_4_BLADE_GRASP = 0x04,
    BATTLE_REACTION_SET_4_ARROW_GUARD = 0x02,
    BATTLE_REACTION_SET_4_HAMEDO = 0x01,
} battle_unit_reaction_set_4_e;

/* Reaction behavior flags indexed by ability ID through the biased linker
 * alias g_main_reaction_behavior_flags_by_ability_id. */
typedef enum battle_reaction_behavior_flag {
    BATTLE_REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY = 0x01,
    BATTLE_REACTION_BEHAVIOR_FLAG_ABILITY = 0x02,
    BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_SELF = 0x04,
    BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER = 0x08,
    BATTLE_REACTION_BEHAVIOR_FLAG_PASSIVE = 0x10,
    BATTLE_REACTION_BEHAVIOR_FLAG_ACTIVE = 0x20,
} battle_reaction_behavior_flag_e;

typedef struct map_move_find_item_entry {
    u8 position; /* X in the high nibble, Y in the low nibble. */
    u8 flags;    /* map_move_find_item_entry_flags_e */
    u8 rare_item_id;
    u8 common_item_id;
} map_move_find_item_entry_t;
typedef char map_move_find_item_entry_size_must_be_4[(sizeof(map_move_find_item_entry_t) == 4) ? 1 : -1];

typedef struct map_move_find_item_data {
    map_move_find_item_entry_t entries[4];
} map_move_find_item_data_t;
typedef char map_move_find_item_data_size_must_be_16[(sizeof(map_move_find_item_data_t) == 16) ? 1 : -1];

extern s32 g_action_type;
extern s32 g_battle_action_at_list_active;
extern s32 g_battle_action_at_list_id;
extern const battle_action_display_image_rect_t g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_COUNT];
extern const u8 g_battle_action_display_numeric_selectors[16];
extern u16 g_battle_action_display_values[16];
extern const u8 g_battle_action_status_display_image_ids[BATTLE_STATUS_COUNT];
extern const u16 g_battle_action_result_palette[16];
extern s32 g_battle_action_phase;
extern s32 g_battle_action_post_action;
extern s32 g_battle_action_post_action_display_phase;
extern s32 g_battle_action_post_effect_msg_counter;
extern battle_action_data_t* g_battle_action_attacker_data;
extern battle_action_state_e g_battle_action_state;
extern battle_action_data_t* g_battle_action_target_data;
extern battle_action_context_e g_battle_action_context;
extern s16 g_battle_current_reaction_ability_id;
extern s32 g_battle_distribute_target_count;
extern s32 g_battle_casting_misc_id;
extern s32 g_battle_casting_unit_id;
extern map_move_find_item_data_t g_battle_current_map_move_find_item_data;
extern u8 g_battle_target_ability_targets_list[16];
extern targeting_panel_entry_t g_battle_target_panel_data[0x200];
extern u8 g_battle_target_tile_targetable_flags[];
extern s32 g_casting_unit_misc_id;
extern battle_current_ability_t g_current_ability;
extern s32 g_current_ability_hamedo_flag;
extern s32 g_current_battle_event_id;
extern s32 g_current_facing_direction;
extern u8 g_reaction_unit_action_data_16e[];
extern battle_stats_t* g_battle_action_target;
extern battle_post_effect_msg_t g_battle_action_post_effect_msgs[];
extern u8 g_battle_action_menu_skillsets[];
extern battle_target_panel_t g_battle_target_panel_last;

/* g_reaction_unit_action_data_16e + 2: the saved battle_stats_t
 * last_ability_id. The container is a raw byte copy of the unit's action
 * fields, so this halfword keeps its own name. */
extern s16 g_reaction_unit_last_ability_id;
extern battle_current_attacker_data_t g_current_ability_attacker;
extern battle_action_data_t g_current_action_data;
extern battle_stats_t* g_battle_action_attacker;
extern u8 g_battle_action_menu_row_types[]; /* per-row skillset kind; 13 keeps the row active */
extern s32 g_battle_action_post_action_unit_id;
extern void (*g_battle_formula_handlers[])(void);
extern u16 g_battle_target_color_tile_x;
extern u16 g_battle_target_color_tile_y;
extern s16 g_battle_target_tile_mark_modes[3];
extern u8 g_current_ability_canceled_statuses[];

/* Typed alias of targeting_panel_data: 512 map panels followed by 16 auxiliary
 * panels, all initialized at 0x80174e84. This declaration allocates no storage. */
extern battle_target_panel_t g_battle_target_panels[];
extern battle_screen_coords_t g_battle_current_misc_screen_coords;
extern VECTOR g_battle_current_vector;
extern u32* g_current_otag_entry;
extern battle_render_buffer_t* g_battle_target_tile_color_buffer;
extern void* g_current_effect_work;
extern u16 g_battle_current_music_track_index; /* 0x80165fd8 */
extern entd_unit_t* g_current_entd_unit;
extern s32 g_battle_current_thread_id;
s32 battle_action_add_poached_item_to_fur_shop_inventory(void);
s32 battle_action_calculate_at_list(battle_at_entry_t* list, s32 mode);
u32 battle_action_get_number_of_turns_to_resolve(s32 unit_index, s32 ct, battle_at_entry_t* entries);
s32 battle_action_calculate_at_list_and_get_specific_unit_id(s32 unit_idx);
s32 battle_action_calculate_chance_to_react(const battle_stats_t* unit);
void battle_action_call_attack_preparation(battle_ai_command_action_t* action);
s32 battle_action_can_unit_react(const battle_stats_t* unit);

/* Provisional: the definition falls off the end after the tail call to
 * battle_formula_can_unit_evade, so v0 carries that result; every caller reads
 * it as an s32 status. */
s32 battle_action_can_unit_react_1(battle_stats_t* unit);
s32 battle_action_check_and_consume_mp(battle_stats_t* unit);
void battle_action_check_arrow_guard_usability(void);
void battle_action_check_between_turn_events(void);
void battle_action_check_blade_grasp_usability(void);
void battle_action_check_counter_reaction_usability(u16 reaction_id, u32 mask);
s32 battle_action_check_change_of_turn(s32 unit_id);
void battle_action_check_face_up_and_absorb_used_mp_usability(s16 reaction_id);
void battle_action_check_critical_quick_hp_restore_mp_restore_meatbone_slash_usability(s16 reaction_id);
void battle_action_check_damage_split_usability(void);
void battle_action_check_distribute_usability(void);
void battle_action_check_mp_switch_usability(void);
void battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(s16 reaction_id);
s32 battle_action_check_reaction(battle_stats_t* unit);
void battle_action_check_reflect_reaction(void);
void battle_action_choose_facing_for_wait(void);
void battle_action_choose_wait(void);
void battle_action_clear_at_list_active(void);
void battle_action_clear_at_list_id(void);
void battle_action_open_status_menu_at_list_start(void);
void battle_action_clear_current_data(battle_action_data_t* action);
void battle_action_clear_data(void);
void battle_action_clear_knockback_flag(void);
void battle_action_clear_status_changes(battle_action_data_t* action);
void battle_action_confirm(void);
void battle_action_copy_active_turn_data_from(const u8* banner, const u8* unit, const u8* billboard);
s32 battle_action_end_turn(s32 unit_id);
void battle_action_execute_ability(void);
void battle_action_finalize_target_current_action(void);
void battle_action_get_next_acting_unit(s32 arg);
void battle_action_handle_move_command(void);
void battle_action_handle_post_action_xp_jp_ability(void);
void battle_action_increment_at_list_id(void);
s32 battle_action_increment_item_quantity_for_steal_break(battle_stats_t* unit, s32 item);
void battle_action_init_learn_ability_on_hit(void);
s32 battle_action_init_movement_ability_benefit(battle_stats_t* unit);
void battle_action_is_at_list_unit_charging_ability(void);
s32 battle_action_report_job_level_up(battle_unit_misc_data_t* misc);
s32 battle_action_report_level_up(battle_unit_misc_data_t* misc);
s32 battle_action_resume_attack_phase_control(void);
void battle_action_run_between_turn_events(void);
void battle_action_set_at_list_active(void);
void battle_action_set_casting_unit_id_ff(void);
void battle_action_set_current_attacker_data(battle_stats_t* unit);
void battle_action_set_damage_display_type_based_on_ability(void);
void battle_action_set_item_throw_stone_ability_display(void);
s32 battle_action_set_move_act_flags(s32 unit_id, s32 move_flag, s32 act_flag);
void battle_action_set_only_action_taken(s32 unit_id);
void battle_action_set_only_movement_taken(s32 unit_id);
void battle_action_set_target_variables(battle_stats_t* unit);
s32 battle_action_should_end_unit_turn(battle_stats_t* unit);
void battle_action_enter_move_range_exception(void);
void battle_action_show_caster_post_effect_messages(void);
void battle_action_switch_ability_to_default_attack(void);
void battle_formula_3c_damage_caster_max_hp_one_fifth_heal_target_two_fifths(void);
void battle_formula_43_damage_caster_missing_hp(void);
void battle_formula_add_brave_x_stats_y(void);
void battle_formula_apply_attack_up_and_martial_arts(void);
void battle_formula_apply_attacker_berserk_frog(void);
void battle_formula_apply_catch(void);
void battle_formula_apply_charge(void);
void battle_formula_apply_damage_and_knockback(void);
void battle_formula_apply_dance_abilities(void);
void battle_formula_apply_defense_up(void);
void battle_formula_apply_elemental(void);
void battle_formula_apply_elemental_absorption(void);
s32 battle_formula_apply_elemental_absorption_and_status(void);

/* Provisional: the definition falls off the end after
 * battle_formula_roll_conditional_status_proc, so v0 carries that result and
 * every caller tests it. */
s32 battle_formula_apply_elemental_absorption_and_status_proc(void);
void battle_formula_apply_elemental_strengthen(void);
void battle_formula_apply_evasion_changes_due_to_statuses(void);
void battle_formula_apply_full_hp_mp_heal(void);
void battle_formula_apply_hp_absorption(void);
void battle_formula_apply_magic_attack_up(void);
void battle_formula_apply_magic_defense_up(void);
void battle_formula_apply_magical_support_status_compatibility(void);
void battle_formula_apply_maintenance(void);
void battle_formula_apply_mp_recovery(void);
void battle_formula_apply_physical_attack_supports(void);
void battle_formula_apply_physical_status_support_compatibility(void);
void battle_formula_apply_physical_xa_modifying_statuses_support(void);
void battle_formula_apply_quick_effect(void);
void battle_formula_apply_song_abilities(void);
void battle_formula_apply_status(void);
void battle_formula_apply_status_to_action(void);
s32 battle_formula_apply_status_and_check_undead(void);
s32 battle_formula_apply_steal_break_might_sword_hardcoding(void);
void battle_formula_apply_target_magical_status_xa_modifiers(void);
void battle_formula_apply_target_xa_affecting_statuses(void);
void battle_formula_apply_undead_absorb_attack(void);
void battle_formula_apply_undead_reversal(void);
void battle_formula_apply_weapon_element_strengthen(void);
void battle_formula_apply_weather_effects_on_bows(void);
void battle_formula_apply_weather_elemental_effects(void);
void battle_formula_apply_y_brave(void);
void battle_formula_calculate_abandon(void);
s32 battle_formula_calculate_attack_evaded(u16 numerator, u16 denominator, u8 miss_type, u8 item_lost);
void battle_formula_calculate_base_xa(void);
void battle_formula_calculate_concentrate(void);
void battle_formula_calculate_critical_hit(void);
void battle_formula_calculate_dark_confuse(void);
void battle_formula_calculate_facing_evade(void);
void battle_formula_calculate_faith(void);
void battle_formula_calculate_final_hit_percent(void);
void battle_formula_calculate_gravi2_damage(void);
void battle_formula_calculate_hp_percent_damage(void);
void battle_formula_calculate_katana_break_chance(void);
s32 battle_formula_calculate_magic_accuracy_without_faith(void);
s32 battle_formula_calculate_magical_evade(void);
void battle_formula_calculate_magical_xa_times_ya(void);
void battle_formula_calculate_mp_percent_damage(void);
void battle_formula_calculate_physical_damage(void);
s32 battle_formula_calculate_physical_evade(void);
void battle_formula_calculate_stolen_gil(void);
void battle_formula_calculate_transparent(void);
void battle_formula_calculate_truth_damage(void);
void battle_formula_calculate_truth_magical_damage(void);
s32 battle_formula_can_unit_evade(battle_stats_t* unit);
s32 battle_formula_calculate_hit(void);

/* Provisional: falls off the end after battle_formula_calculate_hit, whose
 * result stays in v0 for the caller's test. */
s32 battle_formula_calculate_physical_evade_charge(void);
void battle_formula_modify_damage_for_element(s32 element);
void battle_formula_nullify_action(void);
void battle_formula_cause_action_miss(void);
void battle_formula_check_dragon(void);
void battle_formula_clear_nullify_flags(void);
void battle_formula_convert_hp_damage_to_mp_recovery(void);
void battle_formula_determine_reduced_stat(void);
void battle_formula_force_attack_miss(void);
void battle_formula_force_sleeping_target_miss(void);
s32 battle_formula_get_random_0_7fff(void);
void battle_formula_heal_mp_z_times_ten(void);
void battle_formula_init_weapon_xa_ya(void);
void battle_formula_modify_elemental_damage(void);
void battle_formula_roll_conditional_status_proc_inner(void);
s32 battle_formula_roll_conditional_status_proc(void);
s32 battle_formula_roll_random_at_least(s32 limit, s32 threshold);
void battle_formula_select_magic_gun_ability(void);
void battle_formula_set_exp_stolen(void);
void battle_formula_store_ma_and_ma_plus_y_divided_by_two(void);
void battle_formula_store_ma_and_pa_plus_y_divided_by_two(void);
void battle_formula_store_ma_and_x(void);
void battle_formula_store_ma_and_y(void);
void battle_formula_store_magical_evade_values(void);
void battle_formula_store_pa_and_pa_plus_y_divided_by_two(void);
void battle_formula_store_pa_and_weapon_power(void);
void battle_formula_store_pa_and_weapon_power_plus_y(void);
void battle_formula_store_pa_and_x(void);
void battle_formula_store_pa_and_y(void);
void battle_formula_store_reaction_proc_id_and_target(void);
void battle_formula_store_speed_and_x(void);
void battle_formula_store_xa_plus_ya_status_damage(void);
void battle_formula_store_xa_times_ya_damage(void);
void battle_formula_use_hp_damage_as_action_hit_percent(void);
s32 battle_target_set_panels_for_action(u8* action);
s32 battle_action_build_reaction_targets(s32 actor_id, battle_strike_work_t* work, s32* target_count, u8* target_ids);
void battle_target_apply_unit_team_eligibility(s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_is_0xff);
void battle_target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi);
void battle_target_calculate_arc_range(battle_stats_t* unit, u8 range, u8 flags);
void battle_target_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y);
s32 battle_target_calculate_map_for_action(battle_ai_command_action_t* action, u8* flags_3);
void battle_target_calculate_strike_lunge_range(battle_stats_t* unit, u8 flags);
void battle_target_calculate_tile_coords_and_glow_from_at_list(void);
void battle_target_calculate_tile_coords_with_cursor_glow(void);
void battle_target_calculate_weapon_range(battle_stats_t* unit);
void battle_target_can_select_tile(void);
void battle_target_clear_panel_data(void);
void battle_target_clear_panel_spread_flags(void);
void battle_target_disable_green_panel_flags(void);
s32 battle_target_disable_green_panel_on_all_but_target_tile(const u8* source);
void battle_target_gather_x_y_data_for_attacks(battle_unit_misc_data_t* unit);
s32 battle_target_get_unit_id_if_tile_targetable(s32 a0, s32 a1, s32 a2);
void battle_target_move_cursor_to_unit(battle_unit_misc_data_t* unit);
void battle_target_remove_close_range(s32 x, s32 y, s32 range);
void battle_target_select_random_tile_for_random_fire_abilities(void);
void battle_target_select_tile(void);
void battle_target_show_move_range_in_free_cursor(void);
void battle_target_sort_list(s32 target_battle_id);
void battle_target_spread_panels(u8 passes, s32 unused);
void battle_target_store_cursor_casting_unit_name_and_data(void);
void battle_target_store_cursor_unit_name_and_data(void);
s32 battle_target_set_boxes_red(void);
void battle_target_store_cursor_unit_as_preview_target(void);
void battle_target_update_free_cursor_selection(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* selected_unit);

s32 battle_target_validate_height_overlap(
    s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id);

void battle_action_queue_post_effect_messages_for_unit(battle_unit_misc_data_t* unit);
s32 battle_target_calculate_cursor_tile_polygon(battle_screen_coords_t* coords, u8 layer, SVECTOR* quad);
s32 battle_target_set_green_panels_for_action(battle_ai_command_action_t* action);
s32 battle_target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index);
s32 battle_action_prepare_attack(battle_ai_command_action_t* action, battle_ai_command_action_t* target, s32 flag);
s32 battle_action_resolve_ability_strike(s32 misc_unit_id, battle_strike_work_t* work);
void battle_action_init_current_ability_strike_data(battle_stats_t* unit);
s32 battle_reaction_prepare_next(u16* reaction_id);
s32 battle_action_run_between_turn_control(s32 mode);
void battle_formula_calculate_knockback(void);
void battle_formula_apply_zodiac_compatibility(void);
void battle_formula_store_physical_evade_values(void);
s32 battle_action_run_pre_formula_setup(const u8* source, u8 target_id);
s32 battle_action_finalize_attack_and_flag_reactions(s32 misc_id);
s32 battle_target_move_cursor_by_input(void);
void battle_target_set_tile_background_color(s32 mode, s32 unused);
void battle_action_set_target_coords_and_attacker_anim(void);
void battle_action_copy_at_and_cursor_to(void* banner, void* unit, void* billboard, void* cursor_tile);
s32 battle_action_add_war_funds(battle_stats_t* unit, s32 total, u8 item_id);
s32 battle_action_apply_tile_trap(battle_stats_t* unit);
void battle_action_build_targets_post_action_message(void);

/* Preview action calculation entry run on the main stack. */
s32 battle_action_calculate_projected_effect(
    battle_stats_t* attacker, battle_stats_t* target, battle_ai_command_action_t* command);

s32 battle_action_call_attack_preparation_at_preview(u8* arg);
s32 battle_action_check_at_list_for_unit_battle_id(battle_stats_t* unit);
s32 battle_action_check_battle_outcome(void);
void battle_action_check_mp_switch_distribute_and_damage_split_usability(void);
void battle_action_check_reflect_blade_grasp_and_arrow_guard(void);
void battle_action_check_stat_save_and_restore_reaction_usability(void);
s32 battle_action_decrement_player_item_quantity(battle_stats_t* unit, s32 item_id, s32 always_one);
void battle_action_dispatch_target_reaction_ability(void);
s32 battle_action_get_elemental_ability_id(battle_stats_t* unit);
void battle_action_finalize_draw_out_katana_result(battle_stats_t* attacker, battle_strike_work_t* work, s32 hit_count);
void battle_action_handle_steal_exp(battle_stats_t* unit, u8 amount);
s32 battle_action_perform_reaction_ability(void);
s32 battle_action_preview_at_list(battle_stats_t* unit, s32 action, s32 at_list);
s32 battle_action_remove_broken_or_stolen_equipment(void);
void battle_action_run_main_reaction_and_flag_job_level_change(battle_stats_t* unit);
s32 battle_action_select_auto_potion_item(battle_stats_t* unit);
void battle_action_set_casting_unit_id_ff_and_init(void);
s32 battle_action_set_mimic_ability(battle_stats_t* unit);
s32 battle_action_sort_at_list(s32 unit_id, s32 mode, u16 sort_key, battle_at_entry_t* list);
void battle_action_store_ability_data(u8* src);
s32 battle_action_store_counter_ability(battle_stats_t* unit, s8 skillset_id, s16 ability_id, s32 validate_target);
void battle_formula_apply_poach_and_train(void);
s32 battle_formula_calculate_dance_song_hit(void);
s32 battle_formula_calculate_elemental_xa_times_ya(void);
s32 battle_formula_calculate_friendly_magic_accuracy(void);
s32 battle_formula_calculate_golem_accuracy(void);
s32 battle_formula_calculate_magic_accuracy(void);
s32 battle_formula_calculate_magic_accuracy_no_elemental(void);
s32 battle_formula_calculate_physical_accuracy(void);
s32 battle_formula_calculate_physical_status_accuracy(void);
s32 battle_formula_calculate_weapon_damage(void);
s32 battle_reaction_prepare_hamedo_for_pending_action(s32 id, u16* out_ability);
s32 battle_target_apply_reflect(battle_stats_t* unit);
void battle_target_apply_vertical_fixed(s32 x, s32 y);
void battle_target_apply_vertical_tolerance(u8 ref_height, u8 tolerance, s32 single_layer);
s32 battle_target_calculate_ability_range_with_map_parameters(battle_stats_t* unit, u8 range);
s32 battle_target_calculate_for_menu_types(const u8* source);
s32 battle_target_count_hit_by_ability(u8* out);
s32 battle_target_list_units_on_panels(u8* list, battle_stats_t* origin);
s32 battle_target_move_cursor_to_battle_id(u32 battle_id);
void battle_target_project_cursor_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised);
s32 battle_target_run_calculator(const battle_ai_command_action_t* source);
void battle_target_set_ability_panels(const u8* source);
s32 battle_target_set_all_panels_targeted_if_targetable(void);
void battle_target_set_coordinates_for_ability(battle_stats_t* acting, battle_strike_work_t* out);
s32 battle_target_set_weapon_attack_panels(battle_ai_command_action_t* source);
s32 battle_target_set_jump_ability_panels(const u8* source);
s32 battle_target_validate_lunging_target(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id);
s32 battle_target_validate_weapon_target(const battle_ai_command_action_t* source);
void battle_target_build_directional_attack_panels(const void* source, u8 mode);
void battle_target_check_moldball_virus_depth(s16 ability_id);
s32 battle_action_init_current_data(s32 unit_id);
void battle_action_store_acting_unit_data(battle_stats_t* unit);
s32 battle_action_store_target_stats_pointer_data(s32 unit_id);

/* move */
typedef enum battle_move_find_result_flags {
    BATTLE_MOVE_FIND_RESULT_ITEM = 0x01,
    BATTLE_MOVE_FIND_RESULT_TRAP = 0x02,
    BATTLE_MOVE_FIND_RESULT_OCCUPIED = 0x04,
} battle_move_find_result_flags_e;

typedef struct battle_move_find_result_data {
    u8 flags; /* battle_move_find_result_flags_e */
    u8 entry_index;
    u8 rare_item_id;
    u8 common_item_id;
    u8 trap_id; /* battle_trap_id_e */
    s8 occupying_unit_id;
} battle_move_find_result_data_t;
typedef char battle_move_find_result_data_size_must_be_6[(sizeof(battle_move_find_result_data_t) == 6) ? 1 : -1];

/* Ordered class selection at 0x80174630–0x80174700, not movement ability
 * flags or the separate cost-table selector. Water names describe standing
 * height; the English ability labels in external references disagree. */
typedef enum battle_movement_class {
    BATTLE_MOVEMENT_CLASS_FLY = 0,
    BATTLE_MOVEMENT_CLASS_TELEPORT = 1,
    BATTLE_MOVEMENT_CLASS_FLOAT = 2,
    BATTLE_MOVEMENT_CLASS_LAVA = 3,
    BATTLE_MOVEMENT_CLASS_WATER_SURFACE = 4,
    BATTLE_MOVEMENT_CLASS_WATER_DEPTH_ONE = 5,
    BATTLE_MOVEMENT_CLASS_UNDERWATER = 6,
    BATTLE_MOVEMENT_CLASS_NORMAL = 7,
} battle_move_class_e;

typedef enum battle_effective_movement_flags {
    BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK = 0x02,
    BATTLE_EFFECTIVE_MOVEMENT_TELEPORT = 0x08,
    BATTLE_EFFECTIVE_MOVEMENT_IN_WATER = 0x10,
    BATTLE_EFFECTIVE_MOVEMENT_ON_WATER = 0x20,
    BATTLE_EFFECTIVE_MOVEMENT_FLOAT = 0x40,
    BATTLE_EFFECTIVE_MOVEMENT_FLY = 0x80,
} battle_move_effective_flags_e;

/* Acting-unit scratch block at 0x1f800000, pointer at 0x8018f4e0.
 * Only target-proven bytes are named. */
typedef struct battle_move_pathfind_scratch {
    u8 _unused_00[2];
    u8 jump_times_two; /* 0x02; 0x3e when flying / ignoring height */
    u8 _unused_03;
    u8 jump_or_1f;     /* 0x04 */
    u8 jump_half;      /* 0x05 */
    u8 move;           /* 0x06 */
    u8 x;              /* 0x07; battle_stats_t 0x47 */
    u8 y;              /* 0x08; battle_stats_t 0x48 */
    u8 high_elevation; /* 0x09 */
    u8 target_x;       /* 0x0a */
    u8 target_y;       /* 0x0b */
    u8 _unknown_0c;
    u8 unit_id;              /* 0x0d; mount id when riding */
    u8 move_type;            /* 0x0e; battle_move_class_e */
    u8 move_mod;             /* 0x0f */
    u8 movement_3;           /* 0x10 */
    u8 cannot_enter_water;   /* 0x11 */
    u8 will_drown;           /* 0x12 */
    u8 will_sink;            /* 0x13 */
    u8 cannot_stay_on_water; /* 0x14 */
    u8 mountable_chocobo;    /* 0x15 */
    u8 can_ride;             /* 0x16 */
    u8 _unused_17;
    u8 map_max_x;       /* 0x18 */
    u8 map_max_y;       /* 0x19 */
    u8 _unknown_1a;     /* 0x1a; byte at 0x8018f4fc */
    u8 unit_size;       /* 0x1b */
    u8 movement_set_3;  /* 0x1c */
    u8 fly_or_teleport; /* 0x1d */
    u8 _unknown_1e[4];
    u8 stepping_stone;      /* 0x22 */
    u8 ai_propagation_mode; /* 0x23; set only by AI target propagation (0x80178224 clears it) */
    u8 movement_set_2;      /* 0x24 */
    u8 can_pass_lava;       /* 0x25 */
} battle_move_pathfind_scratch_t;

typedef enum battle_movement_set_1 {
    BATTLE_MOVEMENT_SET_1_MOVE_PLUS_1 = 0x80,
    BATTLE_MOVEMENT_SET_1_MOVE_PLUS_2 = 0x40,
    BATTLE_MOVEMENT_SET_1_MOVE_PLUS_3 = 0x20,
    BATTLE_MOVEMENT_SET_1_JUMP_PLUS_1 = 0x10,
    BATTLE_MOVEMENT_SET_1_JUMP_PLUS_2 = 0x08,
    BATTLE_MOVEMENT_SET_1_JUMP_PLUS_3 = 0x04,
    BATTLE_MOVEMENT_SET_1_IGNORE_HEIGHT = 0x02,
    BATTLE_MOVEMENT_SET_1_MOVE_HP_UP = 0x01,
} battle_unit_movement_set_1_e;

typedef enum battle_movement_set_2 {
    BATTLE_MOVEMENT_SET_2_MOVE_MP_UP = 0x80,
    BATTLE_MOVEMENT_SET_2_MOVE_GET_EXP = 0x40,
    BATTLE_MOVEMENT_SET_2_MOVE_GET_JP = 0x20,
    BATTLE_MOVEMENT_SET_2_CANNOT_ENTER_WATER = 0x10,
    BATTLE_MOVEMENT_SET_2_TELEPORT = 0x08,
    BATTLE_MOVEMENT_SET_2_TELEPORT_2 = 0x04,
    BATTLE_MOVEMENT_SET_2_ANY_WEATHER = 0x02,
    BATTLE_MOVEMENT_SET_2_ANY_GROUND = 0x01,
} battle_unit_movement_set_2_e;

typedef enum battle_movement_set_3 {
    BATTLE_MOVEMENT_SET_3_WALK_ON_WATER = 0x80,
    BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER = 0x40,
    BATTLE_MOVEMENT_SET_3_MOVE_ON_LAVA = 0x20,
    BATTLE_MOVEMENT_SET_3_MOVE_UNDERWATER = 0x10,
    BATTLE_MOVEMENT_SET_3_FLOAT = 0x08,
    BATTLE_MOVEMENT_SET_3_FLY = 0x04,
    BATTLE_MOVEMENT_SET_3_SILENT_WALK = 0x02,
    BATTLE_MOVEMENT_SET_3_MOVE_FIND_ITEM = 0x01,
} battle_unit_movement_set_3_e;

/* The 0x80-byte movement-path block at Misc Unit Data 0x9c..0x11b. */
typedef struct battle_walk_path {
    u8 bytes[0x80];
} battle_walk_path_t;

enum {
    BATTLE_MOVEMENT_SAVED_SOURCE_HEIGHT = 1,
    BATTLE_MOVEMENT_SAVED_DESTINATION_HEIGHT = 2,
};

/* Per-tile frontier flags produced by the movement spread pass. These are
 * distinct from map_tile_t::ceiling_depth_and_marks and are consumed by the
 * reachable-tile and path-propagation helpers. */
enum {
    FRONTIER_FLAG_LOW_CORNER = 0x02,
    FRONTIER_FLAG_UNIT_ON_TILE = 0x04,
    FRONTIER_FLAG_REACHABLE = 0x10,
    FRONTIER_FLAG_VALID_DESTINATION = 0x20,
};

/* Composite event work produced after movement. This is separate from the
 * tile-query flags in battle_move_find_result_data_t and the learned movement
 * ability bitsets. Bit 0x0200 has readers but no proven producer. */
typedef enum battle_move_post_event_flags {
    BATTLE_MOVE_POST_EVENT_CRYSTAL_OR_TREASURE = 0x0001,
    BATTLE_MOVE_POST_EVENT_MOVEMENT_BENEFIT = 0x0002,
    BATTLE_MOVE_POST_EVENT_ITEM_FOUND = 0x0004,
    BATTLE_MOVE_POST_EVENT_TRAP_TRIGGERED = 0x0008,
    BATTLE_MOVE_POST_EVENT_CHARGING_CANCEL = 0x0010,
    BATTLE_MOVE_POST_EVENT_MOVE_HP_UP = 0x0020,
    BATTLE_MOVE_POST_EVENT_MOVE_MP_UP = 0x0040,
    BATTLE_MOVE_POST_EVENT_MOVE_GET_EXP = 0x0080,
    BATTLE_MOVE_POST_EVENT_MOVE_GET_JP = 0x0100,
    BATTLE_MOVE_POST_EVENT_MOUNT_STATUS_CHANGE = 0x0400,
} battle_move_post_event_flags_e;

/* Scratch +0x22 has signed halfword arithmetic and explicit low-byte readers
 * at 0x80177880–0x80177944. The byte view preserves that truncation. */
typedef union battle_move_budget {
    s16 value;
    struct {
        u8 low;
        u8 high;
    } bytes;
} battle_move_budget_t;

/* Packed half-height offsets at 0x8018f86e. A missing presence flag preserves
 * that side's previous value when applying a saved candidate at 0x80177b64. */
typedef union battle_move_height_offsets {
    u8 byte;
    struct {
        u8 destination_present : 1;
        u8 destination_height_offset : 3;
        u8 source_present : 1;
        u8 source_height_offset : 3;
    } bits;
} battle_move_height_offsets_t;
typedef char battle_movement_height_offsets_size_must_be_1[(sizeof(battle_move_height_offsets_t) == 1) ? 1 : -1];

/* Shared movement work state addressed through 0x8018f4e4.
 *
 * Propagation at 0x80178ca4 and jump spreading at 0x80177c08 use the same
 * base. Coordinate halfwords have both signed and unsigned readers; cast
 * explicitly for signed comparisons. This is a verified prefix, not a claim
 * about the allocation's full size.
 */
typedef struct battle_move_spread_state {
    map_tile_t* current_tile;                 /* 0x00 */
    map_tile_t* destination_tile;             /* 0x04 */
    battle_target_panel_t* current_panel;     /* 0x08 */
    battle_target_panel_t* destination_panel; /* 0x0c */
    battle_target_panel_t* source_panel;      /* 0x10; selected at 0x8017567c */
    /* 0x14; base-grid or 512 + record panel, set at 0x80177614 */
    battle_target_panel_t* effective_destination_panel;
    s16 reachable_count;                            /* 0x18 */
    s16 tile_index;                                 /* 0x1a */
    s16 destination_index;                          /* 0x1c */
    s16 source_panel_index;                         /* 0x1e; terrain index or 512 + compact record index */
    s16 destination_panel_index;                    /* 0x20; terrain index or 512 + compact record index */
    battle_move_budget_t candidate_remaining_range; /* 0x22 */
    s16 jump_remaining_range;                       /* 0x24; source remaining range minus pass at 0x80176594 */
    u16 tile_x;                                     /* 0x26 */
    u16 tile_y;                                     /* 0x28 */
    s16 tile_level;                                 /* 0x2a */
    u16 level_offset;                               /* 0x2c */
    u16 row_offset;                                 /* 0x2e */
    s16 work_x;                                     /* 0x30 */
    s16 work_y;                                     /* 0x32 */
    s16 work_level;                                 /* 0x34; signed at 0x8017742c, low byte copied at 0x80177fdc */
    /* Signed side differences before water adjustment. Source is exit minus
     * opposite at 0x80175864, reversed at 0x80177358. Destination is opposite
     * minus entry at 0x80175b7c, reversed at 0x801776b0. */
    s16 source_side_height_delta;      /* 0x36 */
    s16 destination_side_height_delta; /* 0x38 */
    s16 x_step;                        /* 0x3a */
    s16 y_step;                        /* 0x3c */
    u8 _unused_3e[2];
    /* 0x40-0x43: whether the source/destination is a unit's compact record
     * (a path above a unit) and that record's index, from panel byte 0x02. */
    u8 source_unit_record_flag;       /* 0x40 */
    u8 source_unit_record_index;      /* 0x41 */
    u8 destination_unit_record_flag;  /* 0x42 */
    u8 destination_unit_record_index; /* 0x43; also the per-step on-record flag in path encoding */
    u8 source_side_shift;
    u8 destination_side_shift;            /* 0x45; slope shift of the destination entry side */
    u8 source_ceiling_height;             /* 0x46; half-height units */
    u8 destination_ceiling_height;        /* 0x47; half-height units */
    u8 source_exit_height;                /* 0x48; half-height units */
    u8 source_opposite_height;            /* 0x49; extended samples at 0x80177180 leave it unchanged */
    u8 destination_entry_height;          /* 0x4a; half-height units */
    u8 destination_opposite_height;       /* 0x4b */
    u8 source_base_height_times_two;      /* 0x4c */
    u8 destination_base_height_times_two; /* 0x4d */
    u8 _unused_4e[2];
    /* 0x50; height scratch: a side height compared with the exit/entry height,
     * a panel max_height_delta, or a unit record index in the jump check. */
    u8 work_height;
    u8 jump_half_height;        /* 0x51; intermediate-panel half height at 0x8017637c */
    u8 jump_slope;              /* 0x52; intermediate-panel packed slope at 0x80176388 */
    u8 jump_unit_size;          /* 0x53; acting-unit body size copied at 0x80176260 */
    u8 outer_count;             /* 0x54 */
    u8 inner_count;             /* 0x55 */
    u8 source_half_height;      /* 0x56; tile byte 3, low five bits */
    u8 destination_half_height; /* 0x57 */
    u8 source_slope;            /* 0x58; packed two-bit side multipliers */
    u8 destination_slope;       /* 0x59 */
    u8 budget_matches;          /* 0x5a; candidate budget equals predecessor budget minus step cost */
    u8 candidate_saved;         /* 0x5b; set by the save-selected-candidate step at 0x8017808c */
    u8 _unknown_5c[6];
    u8 frontier_max_remaining_range; /* 0x62; maximum residual queued for the next pass */
    u8 _unused_63;
    /* 0x64 is |exit - entry| (0 when flying), stored into panel max_height_delta;
     * 0x65-0x67 are the best candidate's tie-breakers (0x80177e64). */
    u8 height_delta;          /* 0x64 */
    u8 best_height_delta;     /* 0x65 */
    u8 best_fly_height_delta; /* 0x66 */
    u8 best_extra_span;       /* 0x67 */
    u8 saved_x;               /* 0x68 */
    u8 saved_y;               /* 0x69 */
    u8 saved_level;           /* 0x6a */
    u8 saved_unit_record_flag;
    u8 saved_unit_record_index;
    u8 saved_remaining_range;
    u8 saved_destination_height_offset;
    u8 saved_source_height_offset;
    u8 saved_height_offset_flags;
    u8 placement_failed; /* 0x71 */
    u8 selected_source_side_shift;
    u8 previous_source_side_shift;
    u8 clearance_height; /* 0x74; ceilings below this plus unit size block the step (0x801777cc) */
} battle_move_spread_state_t;
typedef char
    battle_move_spread_tile_x_must_be_26[((unsigned long)&((battle_move_spread_state_t*)0)->tile_x == 0x26) ? 1 : -1];
typedef char battle_move_spread_frontier_must_be_62
    [((unsigned long)&((battle_move_spread_state_t*)0)->frontier_max_remaining_range == 0x62) ? 1 : -1];
typedef char battle_move_spread_placement_must_be_71
    [((unsigned long)&((battle_move_spread_state_t*)0)->placement_failed == 0x71) ? 1 : -1];

/* Sixteen seven-byte records reached through 0x8018f4e8. AI propagation
 * invalidates byte 3 at 0x80178d50–0x80178d60. The ordinary movement producer
 * at 0x80174430 stores heights in half-height units: body height includes a
 * rider adjustment, standing height includes terrain/water/Float adjustments,
 * and top height is their sum (0x80174a28–0x80174b44). */
typedef struct battle_move_record {
    u8 x;                    /* 0x00 */
    u8 y;                    /* 0x01 */
    u8 higher_elevation : 7; /* 0x02 */
    u8 stepping_stone : 1;
    u8 unit_id_flags;   /* 0x03; 0x1f battle id, 0x20 not mountable, 0x40 not in active team, 0xff none */
    u8 body_height;     /* 0x04 */
    u8 standing_height; /* 0x05 */
    u8 top_height;      /* 0x06 */
} battle_move_record_t;
typedef char battle_movement_record_size_must_be_7[(sizeof(battle_move_record_t) == 7) ? 1 : -1];

/* Contiguous result bytes returned as battle_move_find_result_data_t. Keep
 * distinct symbols: the target reloads their absolute addresses separately. */
extern u8 g_battle_move_find_result_flags;
extern u8 g_battle_move_find_entry_index;
extern u8 g_battle_move_find_rare_item_id;
extern u8 g_battle_move_find_common_item_id;
extern u8 g_battle_move_find_trap_id;
extern s8 g_battle_move_find_occupying_unit_id;
extern u8 g_battle_move_current_edge_height;
extern u8 g_battle_move_destination_edge_height;
extern s32 g_battle_move_jump_gravity;
extern u8 g_battle_move_step_value;
extern s16 g_battle_move_target_screen_z;
extern u8 g_battle_move_entry_edge_centre_offsets[];
extern u8 g_battle_move_step_centre_offsets[];
extern u8 g_battle_move_climb_step_centre_offsets[];
extern u8 g_battle_move_ascent_centre_offsets[];
extern battle_unit_misc_data_t* g_battle_move_tile_occupant;
extern s32 g_battle_move_displacement_direction;
extern u8* g_battle_move_terrain_costs_ptr;
extern s32 g_battle_move_reachable_tiles_valid;
extern s32 g_battle_move_reachable_unit_id;
extern u8 g_battle_move_path[];
extern u8 g_battle_move_pathing_scratch;
extern battle_move_pathfind_scratch_t* g_battle_move_config_ptr;

/* The two map tiles of the step in progress, refreshed by battle_move_get_current_and_destination_tiles from
 * the unit's movement value. Callers that only forward them to the step
 * starters take them as opaque words; the geometry readers use the tile
 * fields (height at 0x02, depth_half_height at 0x03, slope_type, flags_06). */
extern map_tile_t* g_battle_move_current_tile;
extern map_tile_t* g_battle_move_destination_tile;

/* Pending battle_move_post_event_flags_e bits, dispatched in priority order. */
extern s32 g_battle_move_find_result;
extern u8* g_battle_move_frontier_flags_ptr;

/* Seven proven half-height offsets at 0x8018f4d8; the following byte is not
 * established as another element. Extended callers select indices 0..6. */
extern u8 g_battle_move_height_offsets[7];
extern battle_move_record_t* g_battle_move_records_ptr;
extern battle_move_spread_state_t* g_battle_move_scratch_pad_ptr;
extern u8 g_battle_move_path_height_offsets;
extern u8 g_battle_move_destination_unit_record;
extern u8 g_battle_move_effective_flags;
extern u8 g_battle_move_landing_centre_offsets[];
extern u8* g_battle_move_pathing_frontier;
extern s32 g_battle_move_pathing_resume_pass;
extern s32 g_battle_move_pathing_tile_index;
extern void (*g_battle_move_spread_preset_table[])(void);
extern u8 g_battle_move_terrain_cost;

/* Height scratch record at 0x80096238; battle_calculate_unit_height_data fills
 * it in place. */
extern battle_unit_height_data_t g_battle_move_tile_occupant_height;
extern u8 g_battle_move_weather_severity;
s32 battle_move_check_horizontal_jump(s32, u8, u8, u8);

u8* battle_move_calculate_pathing(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x, s32 target_y,
    u32 target_level, s32 initialize, s32* suspended, s32 check_budget);

void battle_move_set_unit_step_slope_scales(battle_unit_misc_data_t* unit);
void battle_move_step_unit_to_map_tile_center(battle_unit_misc_data_t* unit, s32 allow_height_change);
void battle_move_undo_unit_move(battle_unit_misc_data_t* unit);

void battle_move_set_velocity_for_contiguous_steps_with_final_tile_height(
    battle_unit_misc_data_t* unit, const u8* path, const u8* step);

void battle_move_apply_unit_step_velocity(battle_unit_misc_data_t* unit);
void battle_move_displace_unit_at_destination_tile(battle_unit_misc_data_t* unit, s32 direction);
void battle_move_finish_unit_step_at_tile_edge(battle_unit_misc_data_t* unit);
void battle_move_update_walking_step_at_tile_edge(battle_unit_misc_data_t* unit);
s32 battle_move_start_next_post_movement_step(void);
void battle_move_accelerate_unit_to_destination_with_height_change(battle_unit_misc_data_t* unit);
void battle_move_accelerate_unit_to_destination_no_height_change(battle_unit_misc_data_t* unit);
void battle_move_store_unit_movement_to_scratchpad(s32 unit_id);
s32 battle_move_set_reachable_tiles(s32 unit_id, s32 map_x, s32 map_y, s32 map_z);
void battle_move_set_tile_flags_for_pathfinding(s32 mode);
void battle_move_update_path_step(battle_unit_misc_data_t* unit);

void battle_move_get_current_and_destination_tiles(
    battle_unit_misc_data_t* unit, map_tile_t** current_tile, map_tile_t** destination_tile);

void battle_move_check_occupied_tile_standing_height(s32 record_index, s32 extra_span);
void battle_move_advance_display_unit_step(battle_unit_misc_data_t* unit);
void battle_move_apply_selected_candidate(battle_move_spread_state_t* state);
void battle_move_calculate_spread(void);
u8 battle_move_calculate_tile_ceiling(s32 x, s32 y, s32 exit_slope_shift, s32 exit_height);
s32 battle_move_calculate_tile_layer_step_offset(s32 x, s32 y, u32 layer);
s32 battle_move_check_spreading_tile_coordinates(void);
void battle_move_clear_reachable_flags_under_dead_or_jumping_units(void);
void battle_move_displace_overlapping_unit(battle_unit_misc_data_t* unit, s32 direction);
void battle_move_displace_unit_along_step_direction(battle_unit_misc_data_t* unit, s32 x, s32 y, s32 delta);
void battle_move_finalize_path_after_animation(battle_unit_misc_data_t* unit);
battle_move_effective_flags_e battle_move_get_effective_flags(const battle_stats_t* unit);
s32 battle_move_get_support_flags(battle_stats_t* unit);
s32 battle_move_has_reached_current_tile_exit_edge(s32 direction, battle_unit_misc_data_t* unit);
s32 battle_move_has_reached_destination_tile_center(s32 direction, battle_unit_misc_data_t* unit);
s32 battle_move_has_reached_destination_tile_entry_edge(s32 direction, battle_unit_misc_data_t* unit);
s32 battle_move_init_destination_geometry(s32 candidate);
void battle_move_init_knockback(battle_unit_misc_data_t* unit);
void battle_move_init_post_movement_display(void);
s32 battle_move_init_source_geometry(s32 direction);
s32 battle_move_init_source_panel(s32 direction);
void battle_move_init_spread_scratch(void);

s32 battle_move_calculate_jump_arc_velocity(
    const battle_screen_coords_t* from, const battle_screen_coords_t* to, VECTOR* out);

battle_walk_path_t* battle_move_calculate_walkto_pathing(
    s32 a, s32 b, s32 x, s32 y, s32 z, s32 destination_x, s32 destination_y, s32 destination_z);

/* Scales the three velocity words at misc-unit offsets 0x28/0x2c/0x30; the
 * definition indexes them as a word array. */
void battle_move_interpolate_partial(s32* velocity_words, s32 scale);
u32 battle_move_set_target_for_mounted_unit_and_find_item(battle_stats_t* unit);
void battle_move_start_unit_step(battle_unit_misc_data_t* unit, const map_tile_t* from, s16 facing);
void battle_move_start_float_step(battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);
void battle_move_start_unit_step_at_climb_speed(battle_unit_misc_data_t* unit, const map_tile_t* from);
s32 battle_move_is_unit_moving_by_misc_id(u32 misc_id);
s32 battle_move_propagate_destination(s32 candidate, s32 extra_span);
void battle_move_return_to_previous_map_square(battle_unit_misc_data_t* unit);
void battle_move_save_selected_candidate(s32 direction, s32 candidate, s32 extra_span);
void battle_move_set_reachable_tile_flags(void);
void battle_move_set_spreading_tile_x_and_y_coordinates(void);
void battle_move_snap_axis_to_current_tile_exit_edge(s32 direction, battle_unit_misc_data_t* unit);
void battle_move_snap_axis_to_destination_tile_center(s32 direction, battle_unit_misc_data_t* unit);
void battle_move_spread_to_neighbors(void);
s32 battle_move_start_unit_walk_to(s32 misc_id, s16* coordinates, s32 elevation_addend, s32 mode, s32 speed);
void battle_move_step_unit_to_map_tile_center_no_height_change(battle_unit_misc_data_t* unit);
void battle_move_step_unit_to_map_tile_center_with_height_change(battle_unit_misc_data_t* unit);
void battle_move_transfer_tiles_height_halves_and_slope_to_scratch_pad(void);
void battle_move_update_airborne_ascent_phase(battle_unit_misc_data_t* unit);
s32 battle_move_update_candidate(s32 extra_span);
void battle_move_update_knockback_after_animation(battle_unit_misc_data_t* unit);
void battle_move_update_unit_step_to_destination_tile_center(battle_unit_misc_data_t* unit);
s32 battle_move_validate_float_fly(battle_unit_misc_data_t* unit);
void battle_move_apply_knockback(void);
s32 battle_move_calculate_teleport_chances(void);
void battle_move_check_and_spread_one_tile_around(void);
s32 battle_move_check_knockback_destination(s32 direction, s32 x, s32 y);
void battle_move_encode_path_steps(void);
s32 battle_move_get_direction(const battle_unit_misc_data_t* unit);
void battle_move_set_unit_path_flag(battle_unit_misc_data_t* unit);

void battle_move_set_unit_step_delta_center_to_edge(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);

void battle_move_set_float_step_delta_center_to_edge(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);

void battle_move_set_unit_step_delta_edge_to_center(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);

void battle_move_set_float_step_delta_edge_to_center(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);

void battle_move_set_velocity_for_contiguous_clamped_steps(
    battle_unit_misc_data_t* unit, const u8* path, const u8* step);

void battle_move_spread_horizontal_jump(void);
void battle_move_start_unit_climb_hop_step(battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);

void battle_move_start_unit_climb_jump_step(
    battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);

void battle_move_start_float_climb_jump_step(
    battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);

void battle_move_start_unit_jump_step(battle_unit_misc_data_t* unit);
void battle_move_start_fly_step(battle_unit_misc_data_t* unit, s32 current_tile, s32 destination_tile);
void battle_move_transfer_target_coordinates(battle_unit_misc_data_t* src, battle_unit_misc_data_t* dst);
void battle_move_update_knockback_step(battle_unit_misc_data_t* unit);
void battle_move_update_unit_by_misc_id(void);
void battle_move_update_unit_step_to_current_tile_exit_edge(battle_unit_misc_data_t* unit);
void battle_move_update_float_step_to_current_tile_exit_edge(battle_unit_misc_data_t* unit);
void battle_move_update_float_step_to_destination_tile_center(battle_unit_misc_data_t* unit);
void battle_move_update_unit_step_to_destination_tile_entry_edge(battle_unit_misc_data_t* unit);
void battle_move_update_unit_vertical_step_to_destination_tile_center(battle_unit_misc_data_t* unit);
u8* battle_move_build_path_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation);
s32 battle_move_clamp_z_to_tile_headroom(s32 value, s32 x, s32 y, u32 layer);

/* map */
typedef enum battle_trap_id {
    BATTLE_TRAP_ID_DEGENERATOR = 0,
    BATTLE_TRAP_ID_DEATHTRAP = 1,
    BATTLE_TRAP_ID_SLEEPING_GAS = 2,
    BATTLE_TRAP_ID_STEEL_NEEDLE = 3,
    BATTLE_TRAP_ID_NONE = 0xff,
} battle_trap_id_e;

/*
 * Script variable 0x23, set by event instruction {3C} Weather.  The same
 * strength scale covers rain and snow; the map's palette-mod flag word says
 * which one is falling.  battle_map_get_effective_weather folds that flag in by
 * adding 3, so callers that compare against it see the snow values.
 * The equivalent GNS scale is fft_weather_e in libfft.
 */
typedef enum battle_weather {
    BATTLE_WEATHER_NONE = 0,
    BATTLE_WEATHER_NONE_ALT = 1,
    BATTLE_WEATHER_RAIN = 2,
    BATTLE_WEATHER_STORM = 3,
    BATTLE_WEATHER_STRONG_STORM = 4,
    BATTLE_WEATHER_SNOW = 5,
    BATTLE_WEATHER_SNOWSTORM = 6,
    BATTLE_WEATHER_STRONG_SNOWSTORM = 7,
    BATTLE_WEATHER_SNOW_OFFSET = 3,
} battle_map_weather_e;

/* Bits of the map weather flag word at 0x800b6698 ("weather palette mods"). */
typedef enum battle_map_weather_flags {
    BATTLE_MAP_WEATHER_FLAG_SNOW = 0x01,
    BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER = 0x02,
} battle_map_weather_flags_e;

/* Eight 0x0a-byte map texture-animation runtime slots at 0x800911f4, distinct
 * from the 32 mesh texture-animation instructions and their handle table.
 * The slot constants and the serialized/runtime distinction are defined with
 * the map format declarations in map.h. */
typedef struct map_texture_animation_state {
    u8 active;
    u8 elapsed_frames;
    u8 frame_duration;
    u8 _padding_03; /* aligns polygon_group */
    u16 polygon_group;
    u16 first_polygon;
    u16 last_polygon;
} map_texture_animation_state_t;
typedef char map_texture_animation_state_size_must_be_0xa[(sizeof(map_texture_animation_state_t) == 0xa) ? 1 : -1];

/* Weather particle positions at 0x800fc358: weather 0x8f (snow) uses all 64,
 * 0x90 (rain) fills two sets of 32. */
typedef struct battle_weather_particle {
    s16 x;
    s16 y;
    s16 z;
    /* g_battle_map_weather_fall_speed, plus the sprite index for 0x8f; rain
     * reuses it as splash state (0/1/2) and the splash set as a countdown. */
    s16 fall_speed;
} battle_weather_particle_t;

/* Three weather sprite UV rectangles (g_battle_map_weather_drop_uv_rects). */
typedef struct battle_weather_sprite_rects {
    RECT rects[3];
} battle_weather_sprite_rects_t;

enum {
    MAP_TILE_CEILING_DEPTH_MASK = 0x1f,
    MAP_TILE_FLAG_MOVE_DESTINATION = 0x20,
    MAP_TILE_FLAG_MOVE_DESTINATION_SHIFT = 5,
    MAP_TILE_FLAG_ABILITY_RANGE = 0x40,
    MAP_TILE_FLAG_ABILITY_RANGE_SHIFT = 6,
    MAP_TILE_FLAG_TARGETED = 0x80,
};

enum {
    MAP_TILE_FLAG_BLOCKED = 0x01,
    MAP_TILE_FLAG_UNTARGETABLE = 0x02,
    MAP_TILE_FLAG_UNTARGETABLE_SHIFT = 1,
    MAP_TILE_COLLISION_MASK = 0x03,
    MAP_TILE_SHADOW_MODE_SHIFT = 2,
    MAP_TILE_SHADOW_MODE_MASK = 0x0c,
    MAP_TILE_SHADOW_MODE_VALUE_MASK = 0x03,
    MAP_TILE_FLAG_CANNOT_STOP = 0x80,
};

enum {
    MAP_TILE_HALF_HEIGHT_MASK = 0x1f,
    MAP_TILE_DEPTH_VALUE_MASK = 0x07,
    MAP_TILE_DEPTH_SHIFT = 5,
    MAP_TILE_DEPTH_MASK = 0xe0,
};

typedef enum map_tile_camera_block_direction {
    MAP_TILE_CAMERA_BLOCK_NORTHWEST = 0x01,
    MAP_TILE_CAMERA_BLOCK_SOUTHWEST = 0x02,
    MAP_TILE_CAMERA_BLOCK_SOUTHEAST = 0x04,
    MAP_TILE_CAMERA_BLOCK_NORTHEAST = 0x08,
} map_tile_camera_block_direction_e;

enum {
    MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK = 0x0f,
    MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT = 4,
    MAP_TILE_CAMERA_BLOCK_STEEP_MASK = 0xf0,
};

typedef enum gns_map_state_comparison {
    GNS_MAP_STATE_EQUAL = 1,
    GNS_MAP_STATE_REQUESTED_LESS_THAN_RECORD = 2,
    GNS_MAP_STATE_REQUESTED_GREATER_THAN_RECORD = 3,
} gns_map_state_comparison_e;

typedef enum gns_resource_type {
    GNS_RESOURCE_TEXTURE = 0x17,
    GNS_RESOURCE_MESH_PRIMARY = 0x2e,
    GNS_RESOURCE_MESH_OVERRIDE = 0x2f,
    GNS_RESOURCE_MESH_ALTERNATE = 0x30,
    GNS_RESOURCE_END = 0x31,
    GNS_RESOURCE_SET_INDOOR = 0x85,
    GNS_RESOURCE_SET_OUTDOOR = 0x86,
    GNS_RESOURCE_CLEAR_SNOW = 0x88,
    GNS_RESOURCE_SET_SNOW = 0x89,
    GNS_RESOURCE_RESERVED_8A = 0x8a,
    GNS_RESOURCE_SET_WEATHER_MODIFIER = 0x8b,
} gns_resource_type_e;

/* Commands of battle_map_dispatch_map_data_command, which switches on command
 * - 0x6a. This is its own id space: event Use3DObject (0x54) reaches command
 * 0x80. Names come from the case bodies. */
typedef enum map_data_command {
    MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION = 0x6a,
    MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION = 0x6b,
    MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS = 0x71,
    MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS = 0x72,
    MAP_DATA_COMMAND_DISABLE_OVERLAY_DITHER = 0x7a, /* SetDrawMode dtd 0 */
    MAP_DATA_COMMAND_ENABLE_OVERLAY_DITHER = 0x7b,  /* SetDrawMode dtd 1 */
    MAP_DATA_COMMAND_SET_3D_OBJECT_STATE = 0x80,
    MAP_DATA_COMMAND_GET_3D_OBJECT_STATE = 0x81,
    MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE = 0x82,
    MAP_DATA_COMMAND_START_TEXTURE_ANIMATION = 0x83,
    MAP_DATA_COMMAND_SPLIT_MESH_PARTS = 0x98,
    MAP_DATA_COMMAND_RELEASE_GNS_HOLD = 0x9a,
    MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION = 0x9d,
} map_data_command_e;

/* Commands of battle_map_light_state_command. The 0x63-0x65 bodies access
 * g_battle_map_light_direction, but every caller passes or reads the map
 * darkness colour there, so the commands are named for the callers. */
typedef enum map_light_command {
    MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT = 0x56,
    MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT = 0x57,
    MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT = 0x58,
    MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT = 0x59,
    MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR = 0x5a,
    MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR = 0x5b,
    MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR = 0x5c,
    MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR = 0x5d,
    MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR = 0x63,
    MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR = 0x64,
    MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR = 0x65,
} map_light_command_e;

enum {
    MAP_PALETTE_ROW_COUNT = 16,
    MAP_PALETTE_COLORS_PER_ROW = 16,
    MAP_PALETTE_COLOR_COUNT = 256,
};

/* Eight runtime slots rotate the palette colors for map texture animations.
 * They are separate from the 32 serialized 0x14-byte animation instructions
 * described by the Maps/Mesh format. */
enum {
    MAP_TEXTURE_ANIMATION_CAPACITY = 8,
    MAP_TEXTURE_ANIMATION_STATE_BYTES = 0x0a,
};

enum {
    MAP_TILE_SURFACE_FLAG_6 = 0x40,
};

/* Bitfield view of map_tile_t.depth_half_height, for the readers the plain u8
 * spelling cannot reproduce.
 *
 * Reading the byte as a u8 and masking lets GCC fold the two fields into one
 * load and compare the slope with sltiu. The target issues two separate lbu
 * reads and a signed slti (BATTLE 0x80184550 / 0x80184564), which only the
 * bitfield spelling emits.
 *
 * This is deliberately a separate type rather than a change to map_tile_t:
 * that field has more than ten readers whose matches depend on the u8
 * spelling. Reach it as
 * ((map_tile_depth_t*)&tile->depth_half_height)->bits.half_height. */
typedef union map_tile_depth {
    u8 packed;
    struct {
        u8 half_height : 5; /* slope half-height */
        u8 depth : 3;       /* depth added to height */
    } bits;
} map_tile_depth_t;
typedef char map_tile_depth_size_must_be_1[(sizeof(map_tile_depth_t) == 1) ? 1 : -1];

typedef enum map_move_find_item_entry_flags {
    MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR = 0x01,
    MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP = 0x02,
    MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS = 0x04,
    MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE = 0x08,
    MAP_MOVE_FIND_ENTRY_NO_TRAP = 0x10,
    MAP_MOVE_FIND_ENTRY_NO_ITEM = 0x20,
    MAP_MOVE_FIND_ENTRY_UPPER_LAYER = 0x80,
} map_move_find_item_entry_flags_e;

/* Unscaled components decoded from one PlayStation ABBBBBGGGGGRRRRR word. */
typedef struct map_palette_color_components {
    u8 red_5bit;
    u8 green_5bit;
    u8 blue_5bit;
    u8 alpha_bit;
    u8 red_delta_biased;   /* target - current + 0x1f */
    u8 green_delta_biased; /* target - current + 0x1f */
    u8 blue_delta_biased;  /* target - current + 0x1f */
} map_palette_color_components_t;
typedef char map_palette_color_components_size_must_be_7[(sizeof(map_palette_color_components_t) == 7) ? 1 : -1];

typedef struct map_palette_animation_state {
    u8 active;
    u8 blend_step;    /* 0x01; index into g_battle_map_palette_blend_steps_32/_8 */
    u8 delay_counter; /* 0x02; one blend step each time it reaches mode >> 2 */
    u8 mode;
    u8 _unknown_04[4];
} map_palette_animation_state_t;
typedef char map_palette_animation_state_size_must_be_8[(sizeof(map_palette_animation_state_t) == 8) ? 1 : -1];

/* One 0x982-byte runtime bank per loaded mesh, based at 0x800995f4.
 * Components and packed_colors are runtime state/mirror, not the separate
 * 0x200-byte-per-mesh upload staging at 0x800e4ea4. Palette extraction fills
 * both; 0x80092f98 uploads staging and clears the pending word at 0x800995ec. */
typedef struct map_palette_runtime_bank {
    u8 enabled;
    u8 updates_disabled;
    map_palette_animation_state_t animations[MAP_PALETTE_ROW_COUNT];
    map_palette_color_components_t components[MAP_PALETTE_COLOR_COUNT];
    u16 packed_colors[MAP_PALETTE_COLOR_COUNT];
} map_palette_runtime_bank_t;
typedef char map_palette_runtime_bank_size_must_be_0x982[(sizeof(map_palette_runtime_bank_t) == 0x982) ? 1 : -1];

/* Palette runtime state block at 0x800995ec: battle_map_update_animations
 * addresses the banks (0x800995f4) from this base. The upload/move words are
 * the pending flags battle_map_update_lighting (0x80092f98) services. */
typedef struct map_palette_state {
    s32 upload_pending;
    s32 move_pending;
    map_palette_runtime_bank_t banks[14];
} map_palette_state_t;
typedef char map_palette_state_size_must_be_0x8524[(sizeof(map_palette_state_t) == 0x8524) ? 1 : -1];

/*
 * A 20-byte row from the file section of MAPnnn.GNS, before the 0x80
 * separator. Resource type 0x31 marks repeated filler rows. For mesh records,
 * the loader compares map_state against a request assembled from the low 12
 * bits of the script variable at offset 0, weather in bits 12-14, and
 * time-of-day in bit 15.
 *
 * The two bytes at 4 also form the conventional little-endian GNS record type
 * (for example, 0x2e01 for a primary mesh).  The game reads them separately:
 * the low byte selects the state comparison and the high byte selects the
 * resource type. In all 2,987 pre-separator rows of the USA disc, +0x06 is
 * 0x3333 and +0x10..+0x13 are 55 66 77 88. The loader compares those bytes
 * when checking whether a file changed, and tests +0x13 for a nonzero marker.
 */
typedef struct gns_file_record {
    u16 script_variable_id;  /* 0x00 */
    s16 map_state;           /* 0x02 */
    u8 map_state_comparison; /* 0x04; gns_map_state_comparison_e */
    u8 resource_type;        /* 0x05; gns_resource_type_e */
    u16 _padding_06;         /* 0x06; aligns disc_lba */
    u32 disc_lba;            /* 0x08; absolute, including the high halfword at +0x0a */
    u32 byte_length;         /* 0x0c; sector-rounded */
    u8 padding_55_66_77[3];  /* 0x10 */
    u8 file_present_marker;  /* 0x13; 0x88 on disc, tested for nonzero */
} gns_file_record_t;
typedef char gns_file_record_size_must_be_0x14[(sizeof(gns_file_record_t) == 0x14) ? 1 : -1];

/* Command rows after the 0x80 separator reuse the 20-byte prefix, but +0x08
 * is a byte stride (0x14 or 0x1c), not a disc LBA. The 0x1c rows have eight
 * more payload bytes after this prefix. +0x06/+0x09..+0x0b are zero in the
 * USA disc's command rows. */
typedef struct gns_command_record_prefix {
    u16 script_variable_id;  /* 0x00 */
    s16 map_state;           /* 0x02 */
    u8 map_state_comparison; /* 0x04 */
    u8 resource_type;        /* 0x05 */
    u16 _unused_06;
    u8 record_byte_length; /* 0x08 */
    u8 _unused_09;
    u16 _unused_0a;
    u8 payload[8]; /* 0x0c; command arguments, followed by more bytes for 0x1c rows */
} gns_command_record_prefix_t;
typedef char gns_command_record_prefix_size_must_be_0x14[(sizeof(gns_command_record_prefix_t) == 0x14) ? 1 : -1];

/* Mesh files begin with 49 32-bit byte offsets indexed by resource ID (slot = ID * 4).
 * The dispatcher visits IDs 0x10..0x30; slots 0..0x0f and the other reserved
 * words are zero in the USA disc's mesh files. A zero offset means the chunk
 * is absent. MAP000.5 alone has an embedded texture quarter at 0x4c. */
typedef struct map_mesh_file_header {
    u32 unused_resource_offsets_00_0f[16]; /* 0x00 */
    u32 geometry_offset;                   /* 0x40 */
    u32 color_palette_offset;              /* 0x44 */
    u32 _unused_48;                        /* 0x48 */
    u32 texture_quarter_0_offset;          /* 0x4c */
    u32 _unused_50[5];                     /* 0x50 */
    u32 lights_background_offset;          /* 0x64 */
    u32 terrain_offset;                    /* 0x68 */
    u32 texture_animation_offset;          /* 0x6c */
    u32 palette_animation_offset;          /* 0x70 */
    u32 _unused_74[2];                     /* 0x74 */
    u32 grayscale_palette_offset;          /* 0x7c */
    u32 _unused_80[3];                     /* 0x80 */
    u32 mesh_animation_offset;             /* 0x8c */
    u32 animated_mesh_offsets[8];          /* 0x90 */
    u32 polygon_render_properties_offset;  /* 0xb0 */
    u32 _unused_b4[4];                     /* 0xb4 */
} map_mesh_file_header_t;
typedef char map_mesh_file_header_size_must_be_0xc4[(sizeof(map_mesh_file_header_t) == 0xc4) ? 1 : -1];

typedef union battle_map_mesh_terrain_tile {
    u16 packed;
    s16 reset_value;
} battle_map_mesh_terrain_tile_t;

/* Polygon position and normal records copied from the mesh geometry block.
 * Each XYZ vector occupies eight bytes; the first position's spare halfword
 * stores the packed terrain tile. */
typedef struct battle_map_mesh_triangle_positions {
    s16 x0, y0, z0;
    battle_map_mesh_terrain_tile_t terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _unused_16;
} battle_map_mesh_triangle_positions_t;

typedef struct battle_map_mesh_quad_positions {
    s16 x0, y0, z0;
    battle_map_mesh_terrain_tile_t terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _unused_16;
    s16 x3, y3, z3;
    u16 _unused_1e;
} battle_map_mesh_quad_positions_t;

typedef struct battle_map_mesh_triangle_normals {
    s16 x0, y0, z0;
    u16 _unused_06;
    s16 x1, y1, z1;
    u16 _unused_0e;
    s16 x2, y2, z2;
    u16 _unused_16;
} battle_map_mesh_triangle_normals_t;

typedef struct battle_map_mesh_quad_normals {
    s16 x0, y0, z0;
    u16 _unused_06;
    s16 x1, y1, z1;
    u16 _unused_0e;
    s16 x2, y2, z2;
    u16 _unused_16;
    s16 x3, y3, z3;
    u16 _unused_1e;
} battle_map_mesh_quad_normals_t;

/* Per-part start indices and counts stored after the mesh transform data. */
typedef struct battle_map_mesh_part_metadata {
    u8 _unused_00[0x88];
    u16 textured_triangle_start;
    u16 textured_quad_start;
    u16 untextured_triangle_start;
    u16 untextured_quad_start;
    u16 textured_triangle_count;
    u16 textured_quad_count;
    u16 untextured_triangle_count;
    u16 untextured_quad_count;
} battle_map_mesh_part_metadata_t;

/* Halfword-aligned view of a 20-byte GNS record: the target copies these
 * records with lwl/lwr pairs, so the loader's type has no word member. */
typedef struct gns_record_view {
    s16 script_variable_id;
    s16 map_state;
    u8 map_state_comparison;
    u8 resource_type;
    u8 load_info[14];
} gns_record_view_t;
typedef char gns_record_view_size_must_be_0x14[(sizeof(gns_record_view_t) == 0x14) ? 1 : -1];

/* Runtime colour fades stepped by update_map_animations (0x800912a4):
 * g_map_background_gradient_transition at 0x800a1b10, the ambient light at
 * 0x800a1b50 and the darkness at 0x800a1b74. Channels are 16.16 fixed point;
 * the byte fields follow the updater's use. */
typedef struct map_color_transition_channels {
    s32 red;
    s32 green;
    s32 blue;
    s32 red_step;
    s32 green_step;
    s32 blue_step;
} map_color_transition_channels_t;
typedef char
    map_color_transition_channels_size_must_be_0x18[(sizeof(map_color_transition_channels_t) == 0x18) ? 1 : -1];

typedef struct map_color_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 _unknown_04[4];
    map_color_transition_channels_t channels;
} map_color_transition_t;
typedef char map_color_transition_size_must_be_0x20[(sizeof(map_color_transition_t) == 0x20) ? 1 : -1];

/* The background gradient fade: one channel set per gradient colour. colors[]
 * at 0x38 (0x800a1b48/0x800a1b4c) is the applied gradient pair;
 * battle_map_blend_background_gradient_color indexes it from the same base. */
typedef struct map_gradient_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 _unknown_04[4];
    map_color_transition_channels_t channels[2];
    map_color_t colors[2];
} map_gradient_transition_t;
typedef char map_gradient_transition_size_must_be_0x40[(sizeof(map_gradient_transition_t) == 0x40) ? 1 : -1];

/* 0x98-byte map mesh part records at 0x800fbe00. The +0x88..+0x96 start
 * and count halfwords are map_mesh_part_metadata_t's (0x800f4dd4). The
 * vector triples are the rotation, translation and scale channels; element
 * [1] is the current value, [0] the start of the running keyframe and [2]
 * its target or step. */
typedef struct battle_map_mesh_part {
    SVECTOR vectors_a[3]; /* 0x00 */
    SVECTOR vectors_b[3]; /* 0x18 */
    VECTOR vectors_c[3];  /* 0x30; reset to {0x1000, 0x1000, 0x1000} */
    MATRIX matrix_60;     /* 0x60 */
    s16 value_80;         /* 0x80 */
    s16 value_82;         /* 0x82 */
    u8 value_84;          /* 0x84 */
    u8 value_85;          /* 0x85 */
    u8 parent;            /* 0x86; parent part, from mesh properties byte 0 */
    u8 value_87;          /* 0x87 */
    u16 starts[4];        /* 0x88 */
    u16 counts[4];        /* 0x90 */
} battle_map_mesh_part_t;
typedef char battle_map_mesh_part_size_must_be_0x98[(sizeof(battle_map_mesh_part_t) == 0x98) ? 1 : -1];

/* One 0x50-byte map mesh animation keyframe (MAPnnn.GNS record type 1).
 * Keyframe ids are 1-based; entry 0 is never loaded. Each of the nine channels
 * (rotation, translation and scale of a mesh part) has a target value, a
 * start/end angle for the sine modes and a mode flag word whose bits 0 and 1
 * are TweenTo and TweenBy. */
typedef struct battle_map_mesh_keyframe {
    SVECTOR vector_0;   /* 0x00; rotation */
    SVECTOR vector_8;   /* 0x08; translation */
    s16 value_10;       /* 0x10; scale x */
    s16 value_12;       /* 0x12; scale y */
    s16 value_14;       /* 0x14; scale z */
    s16 _unused_16;     /* 0x16 */
    s16 angle_start[9]; /* 0x18 */
    s16 angle_end[9];   /* 0x2a */
    u16 flags[9];       /* 0x3c */
    u16 _unused_4e;     /* 0x4e */
} battle_map_mesh_keyframe_t;
typedef char battle_map_mesh_keyframe_size_must_be_0x50[(sizeof(battle_map_mesh_keyframe_t) == 0x50) ? 1 : -1];

/* One mesh animation instruction (MAPnnn.GNS record type 2). A duration of
 * 0 stops the part's animation when it is reached. */
typedef struct battle_map_mesh_instruction {
    u8 keyframe;  /* 1-based keyframe id */
    u8 next;      /* index of the following instruction */
    u16 duration; /* frames */
} battle_map_mesh_instruction_t;

/* A mesh part's 8 animation states of 17 instructions (index 0 unused).
 * States are loaded state-major; row 0 is also the running copy that event
 * command 0x80 overwrites with the selected state. */
typedef struct battle_map_mesh_instructions {
    battle_map_mesh_instruction_t states[8][17];
} battle_map_mesh_instructions_t;
typedef char
    battle_map_mesh_instructions_size_must_be_0x220[(sizeof(battle_map_mesh_instructions_t) == 0x220) ? 1 : -1];

/* Provisional runtime copy of one 0x14-byte map texture animation
 * instruction (32 rows).
 * UV animations move a canvas-sized frame through VRAM with the DR_MOVE
 * packet of the same index; palette animations reload CLUT row
 * canvas_x / 16 from the palette frames at g_battle_map_palette_animation_frames. */
typedef struct battle_map_texture_animation {
    s16 canvas_x;   /* 0x00 */
    s16 canvas_y;   /* 0x02 */
    s16 width;      /* 0x04 */
    s16 height;     /* 0x06 */
    s16 frame_x;    /* 0x08 */
    s16 frame_y;    /* 0x0a */
    u8 column;      /* 0x0c */
    u8 row;         /* 0x0d */
    u8 mode;        /* 0x0e: animation technique / playback state */
    u8 frame_count; /* 0x0f */
    u8 frame;       /* 0x10 */
    u8 duration;    /* 0x11 */
    u8 timer;       /* 0x12 */
    u8 active;      /* 0x13 */
} battle_map_texture_animation_t;
typedef char battle_map_texture_animation_size_must_be_0x14[(sizeof(battle_map_texture_animation_t) == 0x14) ? 1 : -1];

/* Lightning flash state at 0x800f79c0 (g_battle_map_lightning_state).
 * battle_map_set_light_matrices fills color_matrices[0] and [2]; the lightning weather commands clear the rotation part
 * of color_matrices[1] and start the 60-frame flash timer. */
typedef struct battle_lightning_state {
    u32 timer;                  /* 0x00 */
    MATRIX color_matrices[3];   /* 0x04 */
    u8 _unused_64[0x20];        /* 0x64 */
    map_color_t base_colors[2]; /* 0x84 */
    map_color_t colors[2];      /* 0x8c */
} battle_lightning_state_t;
typedef char battle_lightning_state_size_must_be_0x94[(sizeof(battle_lightning_state_t) == 0x94) ? 1 : -1];

/* Record header parsed by the map-state loader at 0x800f26bc: a type word,
 * an entry count and the packed entries. */
typedef struct battle_map_state_record {
    u32 type;
    u32 count;
    u8 data[4];
} battle_map_state_record_t;

extern u8 g_battle_map_background_color[];
extern u8 g_battle_map_background_color_backup[];
extern void* g_battle_map_data_load_buffer;
extern s32 g_battle_map_data_load_complete;
extern s32 g_battle_map_deep_dungeon_load_in_progress;
extern u8 g_battle_map_file_table[];
extern u8 g_battle_map_first_gns_resource_type;
extern u32 g_battle_map_gns_lba[];
extern s32 g_battle_map_gns_record_cursor;
extern gns_file_record_t g_battle_map_gns_records[];
extern s32 g_battle_map_id;
extern u8 g_battle_map_max_x;
extern u8 g_battle_map_max_y;
extern s32 g_battle_map_mesh_data_buffer;
extern void* g_battle_map_mesh_load_buffer;
extern s32 g_battle_map_mesh_load_state;
extern map_move_find_item_data_t g_battle_map_move_find_item_data[];
extern u16 g_battle_map_palette_modified_colors[][0x100];
extern s32 g_battle_map_resource_load_state;
extern s32 g_battle_map_texture_animation_handles[];
extern map_texture_animation_state_t g_battle_map_texture_animation_states[];
extern s32 g_battle_map_textured_quad_count;
extern s32 g_battle_map_textured_quad_render_record_count;
extern s32 g_battle_map_textured_triangle_count;
extern s32 g_battle_map_textured_triangle_render_record_count;
extern s32 g_battle_map_tilt_target;
extern s32 g_battle_map_untextured_quad_count;
extern s32 g_battle_map_untextured_quad_render_record_count;
extern s32 g_battle_map_untextured_triangle_count;
extern s32 g_battle_map_untextured_triangle_render_record_count;
extern s32 g_battle_map_zoom_target;
extern u8 g_map_max_x;
extern u8 g_map_max_y;
extern u16 g_battle_map_weather_particle_palette[];
extern MATRIX g_battle_map_light_color_matrix;
extern MATRIX g_battle_map_light_matrix;
extern SVECTOR g_battle_map_light_rotation;
extern s32 g_battle_map_back_color_red;
extern s32 g_battle_map_back_color_green;
extern s32 g_battle_map_back_color_blue;
extern s16 g_battle_map_weather_effect_mode;
extern VECTOR g_battle_map_camera_zoom;
extern u8 g_battle_map_back_color_disabled;
extern MATRIX g_battle_map_camera_matrix;
extern s32 g_battle_map_command_0x96_frame;
extern MATRIX g_battle_map_light_rotation_matrix;

/* Untextured map polygons go to OT slot (otz & mask) | fixed index: {0, 4}
 * normally, {-1, 0} while map effect data is active (command 0x88), so a
 * non-zero mask also marks that state. */
extern s32 g_battle_map_untextured_fixed_ot_index;
extern s32 g_battle_map_untextured_otz_mask;
extern s32 g_battle_map_command_0x96_duration;
extern s16 g_battle_map_destruction_wait_state;
extern u16 g_battle_map_refresh_pending;
extern map_selected_tile_t g_battle_map_selected_tile_data;
extern u8 g_battle_tile_info_image_location[];
extern world_gfx_image_load_parameters_t g_battle_tile_info_image_params[2];
extern CVECTOR g_battle_map_white_cvector;

/* Default vectors shared with battle_process_map_gns_record, which emits them. */
extern SVECTOR g_battle_map_zero_svector;
extern VECTOR g_battle_map_unit_scale_vector;
extern u8 g_battle_map_weather_primary_speed;
extern u8 g_battle_map_rain_secondary_fall_speed;
extern battle_render_buffer_t* g_battle_map_ambient_restored_buffer;
extern battle_weather_particle_t g_battle_map_rain_splash_particles[32];
extern battle_weather_sprite_rects_t g_battle_map_weather_drop_uv_rects;
extern battle_weather_particle_t g_battle_map_weather_particles[64];
extern u16 g_battle_map_append_state; /* 0x80165fe0 */

/* 1 starts, 2 polls, >=3 is reset to zero by the outer update. */
extern u16 g_battle_map_reload_state; /* 0x80165fde */

/* Battle terrain: two layers of 256 records, indexed
 * (layer * 256 + y * width + x). Callers that walk it with raw byte offsets
 * cast to u8*; the record shape is map_tile_t. */
extern map_tile_t g_battle_map_tile_data[];
extern map_palette_state_t g_battle_map_palette_state;

/* GNS records selected for loading and the ones currently loaded, indexed by
 * resource type. */
extern gns_record_view_t g_battle_map_pending_gns_records[49];
extern gns_record_view_t g_battle_map_loaded_gns_records[49];

/* The applied background gradient pair at 0x801251c8. The copies at
 * 0x800e81d0, 0x800e8228 and 0x800e827c move each colour as an independent
 * unaligned word (lwl/lwr + swl/swr). */
extern map_background_gradient_colors_t g_battle_map_background_gradient_colors;
extern map_color_t g_battle_map_ambient_light_color;
extern map_color_t g_battle_map_darkness_color;
extern map_gradient_transition_t g_map_background_gradient_transition;
extern map_color_transition_t g_battle_map_ambient_light_transition;
extern map_color_transition_t g_battle_map_darkness_transition;
extern battle_map_mesh_part_t g_battle_map_mesh_parts[9];
extern battle_map_mesh_keyframe_t g_battle_map_mesh_animation_keyframes[];
extern battle_map_mesh_instructions_t g_battle_map_mesh_animation_instructions[9];
extern battle_map_texture_animation_t g_battle_map_texture_animations[32];
extern DR_MOVE g_battle_map_texture_animation_moves[32];
extern SVECTOR g_battle_map_camera_rotation;
extern battle_lightning_state_t g_battle_map_lightning_state;
extern u8 g_battle_map_ambient_polygon_color[3];
extern u8 g_battle_map_back_color_bytes[3];
extern SVECTOR g_battle_map_command_0x96_rotation;
extern u8 g_battle_map_gns_dispatch_held;
extern s32 g_battle_map_gns_load_phase;
extern u8 g_battle_map_light_direction[4];
extern u16 g_battle_map_light_transition_command;               /* pending per-frame light command, 0 when idle */
extern u16 g_battle_map_light_transition_duration;              /* transition duration in frames */
extern u16 g_battle_map_light_transition_frame;                 /* transition frame counter */
extern MATRIX g_battle_map_light_transition_start_color_matrix; /* transition start colour matrix */
extern MATRIX g_battle_map_light_transition_start_light_matrix; /* transition start light matrix */

extern MATRIX
    g_battle_map_light_transition_target_color_matrix; /* transition target (0x66) or delta (0x68) colour matrix */

extern MATRIX
    g_battle_map_light_transition_target_light_matrix; /* transition target (0x66) or delta (0x68) light matrix */

extern u8 g_battle_map_lighting_pending;
extern s32 g_battle_map_lightning_mode;
extern s32 g_battle_map_loaded_id;
extern MATRIX g_battle_map_local_light_matrix;
extern s32 g_battle_map_lowest_surface_y;
extern s32 g_battle_map_mesh_part_animation_states[];
extern u8* g_battle_map_mesh_part_geometry[9];
extern u16 g_battle_map_palette_animation_frames[][16];
extern u8 g_battle_map_palette_blend_steps_32[][32];
extern u8 g_battle_map_palette_blend_steps_8[][8];
extern u8 g_battle_map_palette_pending;
extern u32 g_battle_map_palette_upload_words[];
extern battle_map_mesh_triangle_positions_t* g_battle_map_part_textured_triangle_positions;
extern battle_map_mesh_quad_positions_t* g_battle_map_part_textured_quad_positions;
extern battle_map_mesh_triangle_positions_t* g_battle_map_part_untextured_triangle_positions;
extern battle_map_mesh_quad_positions_t* g_battle_map_part_untextured_quad_positions;
extern battle_map_mesh_triangle_normals_t* g_battle_map_part_textured_triangle_normals;
extern battle_map_mesh_quad_normals_t* g_battle_map_part_textured_quad_normals;
extern s32 g_battle_map_part_textured_quad_count;
extern s32 g_battle_map_part_textured_triangle_count;
extern s32 g_battle_map_part_untextured_quad_count;
extern s32 g_battle_map_part_untextured_triangle_count;
extern battle_map_mesh_triangle_positions_t g_battle_map_textured_triangle_positions[];
extern battle_map_mesh_quad_positions_t g_battle_map_textured_quad_positions[];
extern battle_map_mesh_triangle_positions_t g_battle_map_untextured_triangle_positions[];
extern battle_map_mesh_quad_positions_t g_battle_map_untextured_quad_positions[];
extern battle_map_mesh_triangle_normals_t g_battle_map_textured_triangle_normals[];
extern battle_map_mesh_quad_normals_t g_battle_map_textured_quad_normals[];
extern u8 g_battle_map_pending_lighting_data[0x2d];
extern u8 g_battle_map_pending_palette_data[0x200];
extern u8 g_battle_map_polygon_flag_clear_countdown;
extern s32 g_battle_map_primary_textured_quad_count;
extern s32 g_battle_map_primary_textured_triangle_count;
extern s32 g_battle_map_primary_untextured_quad_count;
extern s32 g_battle_map_primary_untextured_triangle_count;
extern s32 g_battle_map_saved_back_color_blue;
extern s32 g_battle_map_saved_back_color_green;
extern s32 g_battle_map_saved_back_color_red;
extern map_color_t g_battle_map_saved_background_gradient_first;
extern map_color_t g_battle_map_saved_background_gradient_second;
extern MATRIX g_battle_map_saved_light_color_matrix; /* saved colour matrix (command 0x5f/0x60) */
extern MATRIX g_battle_map_saved_light_matrix;       /* saved light matrix (command 0x5f/0x60) */
extern u8 g_battle_map_saved_texture_animation_modes[];
extern u8 g_battle_map_texture_pending;
extern u8 g_battle_map_texture_quarter_0_pending;
extern u8 g_battle_map_texture_quarter_1_pending;
extern u8 g_battle_map_texture_quarter_2_pending;
extern u8 g_battle_map_texture_quarter_3_pending;
extern map_tile_t* g_battle_map_tile_data_ptr;
extern u8 g_battle_map_tile_depth;
extern u8 g_battle_map_tile_width;
extern RECT g_battle_map_vram_load_rect;
extern u8 g_battle_map_weather_fall_speed;

/* Map weather flags: bit 1 suppresses weather, bit 0 marks snow. */
extern s32 g_battle_map_weather_flags;
extern MATRIX g_battle_map_weather_matrix;
extern SVECTOR g_battle_map_weather_rotation;
extern VECTOR g_battle_map_weather_scale;
extern s32 g_battle_map_weather_sprite_scale;
extern VECTOR g_battle_map_weather_translation;
void battle_map_blend_darkness_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue);
s32 battle_map_calculate_location(battle_stats_t* unit);
s32 battle_map_calculate_move_find_item_flag(s32 map_id, s32 tile_index, s32 mode);
s32 battle_map_get_tile_data_value(s32 field, s32 x, s32 y, s32 layer);
void battle_map_init_ambient_light(s32 unused_0, s32 unused_1, s32 map_id);
void battle_map_init_background_gradient(s32 unused_0, s32 unused_1, s32 map_id);
void battle_map_start_darkness_blend(s32 first, s32 second, s16 third, s16 fourth, s16 fifth);
s32 battle_map_check_deep_dungeon_crystal_and_init_state(void);
void battle_map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue);

/* Only SET/GET define a result (stored RGB pointer, or null for SET while
 * disabled). Other command paths leave the target return register unspecified. */
s32* battle_map_control_gte_background_color(s32 command, s32 red, s32 green, s32 blue);
void battle_map_polygon_flag_command(s32 command);
MATRIX* battle_map_light_matrix_command(s32 command, MATRIX* color, MATRIX* light, s32 duration);
void battle_map_data_load_complete_thread(void);
s32 battle_map_dispatch_gns_resource(u8 resource_type, u8* data);
void battle_map_extract_palette_colors(const u16* palette, s32 mesh_index, s32 palette_row_index);
s32 battle_map_get_effective_weather(void);
map_tile_t* battle_map_get_tile_data_pointer(s32 x, s32 y, u32 layer);
map_tile_t* battle_map_get_tile_data_ptr_from_misc_id(u32 misc_id);
void battle_map_init_palette_state(void);
s32 battle_map_load_data_stage_0x75(void);
s32 battle_map_load_data_stage_0x76(void);
void battle_map_load_move_find_item_data(map_move_find_item_data_t* map_data);
void battle_map_mark_data_load_complete(void);
void battle_map_modify_background_gradient(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_reset_texture_animations(void);
void battle_map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors);
void battle_map_set_command_0x96_duration(s32 id, s32 value);
void battle_map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_set_darkness_color(s32 frame_duration, const map_color_t* color);
s32 battle_map_step_init_sequence(s32 map_id, s32 step);
s32 battle_map_stop_texture_animation(s32 handle);
void battle_map_store_modified_palette_colors(s32 palette_group, s32 palette_index, s32 store_all);
void battle_map_store_selected_tile_coordinates(void* destination);
void battle_map_store_selected_tile_data(void);
s32 battle_map_try_start_deep_dungeon_mesh_load(void);
s32 battle_map_update_deep_dungeon_and_animation(void);
void battle_map_wait_for_refresh(void);

void battle_map_modify_palette(
    s32 command, s32 amount, s32 palette_index, s32 color_index, s32 all_colors, u16 red, u16 green, u16 blue);

s32 battle_map_calculate_slope_height(const s16* screen_coordinates, u8 map_z);
s32 battle_map_resolve_rotated_offset_layer(battle_stats_t*, s32, s32, s32, s32*, s32*, s32*);
s32 battle_map_load_data(s32 map_id, s32 kind);
void battle_map_blend_background_gradient_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue);
void battle_map_blend_ambient_light_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue);
void battle_map_update_palette_colors_inner(s32 mode, s32 palette_group, s32 palette_index, const u16* colors);
void* battle_map_determine_rare_common_item(battle_stats_t* unit);
battle_move_find_result_data_t* battle_map_set_item_trap_data(u8 x, u8 y, u8 layer);
s32 battle_process_map_gns_record(s32 phase, gns_command_record_prefix_t* record);
void battle_map_update_animations(void);
void battle_map_update_lighting(void);
void battle_map_draw_mesh_and_weather(MATRIX* camera_matrix);
void battle_map_freeze(void);
void battle_map_unfreeze(void);
s32 battle_map_dispatch_map_data_command(s32 command, u32 index, s32 object_state, s32 flag);
void battle_map_light_thread(void);
void battle_map_transition_ambient_light_color(s32 frame_duration, const map_color_t* color);

void battle_map_transition_and_store_background_gradient(
    s32 frame_duration, const map_background_gradient_colors_t* colors);

void battle_map_transition_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors);
void battle_map_transition_darkness_color(s32 frame_duration, const map_color_t* color);

/* Map data, palette and mesh interfaces implemented in the battle overlay. */
void battle_map_init_darkness(s32 unused_0, s32 unused_1, s32 map_id);
s32 battle_map_is_gns_record_matching_state(s32 comparison, s16 record_state, s16 requested_state);
void battle_map_load_palette_data(const u16* palette, s32 mesh_index, s32 palette_row_index, s32 load_all_rows);
s32 battle_map_start_file_load(void* unused_file_table, u8* load_info, s32 destination, s32 suppress_loading_display);
s32 battle_map_stop_texture_animations_and_weather_sfx(void);
void battle_map_store_max_coordinates(SVECTOR* destination);

void battle_map_update_palette_colors(
    s32 mode, s32 palette_group, s32 palette_index, s32 update_all, const u16* colors);

u8* battle_map_load_saved_state_record(battle_map_state_record_t* record);
u8* battle_map_light_state_command(s32 command, u8* data);
void battle_map_append_mesh_geometry(u16* geometry_data, battle_map_mesh_part_metadata_t* metadata);
void battle_map_copy_xy_coords_and_tile_data(u8* p);
map_tile_t* battle_map_get_tile_data_ptr_from_battle_id(u32 battle_id);
map_tile_t* battle_map_get_tile_data_ptr_from_misc_screen_coords(u32 misc_id);
s32 battle_map_get_weather_severity(void);
s32 battle_map_init_units_sprites_event_and_music(s32 map_id, s32 step, s32 deployment_running);
s32 battle_map_load_mesh_variant(s32 mesh_slot);
void battle_map_queue_textured_quads(u32* otag, void* prims, s32 depth, s32* count);
void battle_map_queue_textured_triangles(u32* otag, void* prims, s32 depth, s32* count);
void battle_map_queue_untextured_quads(u32* otag, void* prims, s32 depth, s32* count);
void battle_map_queue_untextured_triangles(u32* otag, void* prims, s32 depth, s32* count);
s32 battle_map_refresh_deep_dungeon(void);
s32 battle_map_command_set_3d_object_state(s32 value_a, s32 value_b);
s32 battle_map_command_start_texture_animation(s32 value_a, s32 value_b);
void battle_map_set_light_matrices(MATRIX* first, MATRIX* second);
void battle_map_set_weather_texture_overlay(s32 command);
s32 battle_map_start_texture_animation(s32 frame_duration, s32 polygon_group, s32 first_polygon, s32 last_polygon);
void battle_map_update_mesh_part_animations(void);

void battle_map_update_palette_from_other(
    const u16* source, s32 section_id, s32 unit_misc_id, s32 preset, s16 red, s16 green, s16 blue, s32 final_value);

void battle_map_update_texture_animations(void);
void battle_map_init_mesh_render_records(u16* geometry_data);
void battle_map_init_weather_particles(s32 command);
gns_file_record_t* battle_map_load_gns_and_move_find_items(s32 map_id);
void battle_map_reset_mesh_render_state(void);

/* menu */
typedef enum battle_menu_system_command {
    BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE = 5,
    BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT = 7,
} battle_menu_system_command_e;

/* One 17-byte per-unit menu record, one per battle-unit slot.
 * battle_menu_reset_unit_records (0x8013f8b4) proves the 17-byte stride and
 * 21 entries and marks byte 1 unset (0xFF); battle_menu_restore_selection_for_selected_unit and
 * battle_menu_record_selection_for_selected_unit (0x8014088c) treat byte 0 as
 * the unit's remembered selection for a tracked menu. */
typedef struct battle_menu_record {
    u8 bytes[17];
} battle_menu_record_t;
typedef char battle_menu_record_size_must_be_0x11[(sizeof(battle_menu_record_t) == 0x11) ? 1 : -1];

/* One row of the 31-entry menu table at g_battle_menu_id_records; its first 12 rows are the
 * menus whose selection is remembered per unit.
 * battle_menu_restore_selection_for_selected_unit and battle_menu_record_selection_for_selected_unit
 * prove the 8-byte stride and read only the leading menu id;
 * battle_menu_resolve_selection walks all 31 rows. */
typedef struct battle_menu_id_record {
    u16 menu_id;
    u16 _padding_02; /* aligns map */
    /* 0x04: per-menu command remap table. map[0] handles the 0xFF selection
     * and map[n + 1] any other value; named by battle_menu_resolve_selection,
     * its only reader. The other readers use menu_id alone. */
    u8* map;
} battle_menu_id_record_t;
typedef char battle_menu_id_record_size_must_be_0x8[(sizeof(battle_menu_id_record_t) == 8) ? 1 : -1];

/* The menu threads' view of one 0x3c-byte menu record (world_menu_entry_t).
 * BATTLE and event-overlay menu threads receive an entry of
 * g_battle_menu_thread_menu_data as parameter 1 (battle_menu_start_mini_menu_display_thread
 * passes &g_battle_menu_thread_menu_data[10]) and read 0x0a as flags and
 * 0x30 as their redraw-flag pointer, where WORLD's record keeps window_y and
 * text_binding; other fields use world_menu_entry_t's names.
 * battle_menu_build_idle_action_menu (0x8013cf58) proves the stride and stores
 * window_x at 0x08 (0x80 plus a per-entry byte picked from the unit's screen
 * position). The halfword at 0x38 holds the entry's current selection
 * (menu struct 0x38): battle_menu_restore_selection_for_selected_unit and
 * battle_menu_record_selection_for_selected_unit move it to and from
 * battle_menu_record_t byte 0, battle_menu_confirm_auto_battle_selection_thread
 * (0x801416a0) reads it, and battle_menu_clear_idle_action_menu_entry_flags
 * (0x8013f76c) clears it. */
typedef struct battle_menu_idle_action_entry {
    u8 _unused_00[4];
    s16 inner_width;  /* 0x04; also stored to g_menu_inner_window_width */
    s16 inner_height; /* 0x06 */
    s16 window_x;     /* 0x08 */
    u16 flags_0a;     /* 0x0a; the menu threads clear bit 0 on entry */
    u8 _unused_0c[2];
    s16 window_height; /* 0x0e; set with inner_height by battle_menu_build_skillset_entries */
    u8 _unused_10[6];
    s16 overall_height; /* 0x16; set with inner_height by battle_menu_build_skillset_entries */
    u8 _unused_18[4];
    s16 text_id;     /* 0x1c; text entry drawn into the window */
    s16 last_option; /* 0x1e; highest option index (battle_menu_update_selection_from_input) */
    u16 value_20;    /* 0x20; set from the system-function table by battle_menu_run_system_function_thread */
    u8 _unused_22[6];
    void (*thread_entry)(void); /* 0x28; menu thread started by battle_menu_start_entry_thread */
    u8 _unused_2c[4];
    s16* refresh_flag; /* 0x30; the menu threads redraw while *refresh_flag == 1 */
    u8 _unused_34[4];
    s16 selected_index; /* 0x38 */
    u8 _padding_3a[2];  /* tail padding to 4-byte alignment */
} battle_menu_idle_action_entry_t;
typedef char battle_idle_action_menu_entry_refresh_must_be_0x30
    [((unsigned long)&((battle_menu_idle_action_entry_t*)0)->refresh_flag == 0x30) ? 1 : -1];
typedef char
    battle_idle_action_menu_entry_size_must_be_0x3c[(sizeof(battle_menu_idle_action_entry_t) == 0x3c) ? 1 : -1];

/* Menu allocator slot (16 at g_battle_menu_allocator_slot_records); buffer is
 * -1 when the slot is free. The slots hand out runs of the 53 256-byte pages
 * of g_battle_menu_buffer_arena. */
typedef struct battle_menu_allocation {
    u8* buffer;     /* 0x00 */
    s32 page_count; /* 0x04 */
} battle_menu_allocation_t;
typedef char battle_menu_allocation_size_must_be_8[(sizeof(battle_menu_allocation_t) == 8) ? 1 : -1];

/* One 12-byte entry of a menu number table drawn by
 * battle_menu_draw_numeric_display_entries; kind 0 and 1 select the plain
 * and coloured number renderers. */
typedef struct menu_number_entry {
    u16 x;      /* 0x00 */
    u16 y;      /* 0x02 */
    s16* value; /* 0x04 */
    s16 digits; /* 0x08; digit count, 0x400/0x800/0xc00/0x1000 sign flags */
    s16 kind;   /* 0x0a */
} menu_number_entry_t;
typedef char menu_number_entry_size_must_be_12[(sizeof(menu_number_entry_t) == 12) ? 1 : -1];

/* Text origin rewritten from each menu_number_entry_t before it is drawn. */
typedef struct menu_number_position {
    u16 x; /* 0x00 */
    u16 y; /* 0x02 */
} menu_number_position_t;

/* BATTLE scratch at 0x801cc074, in the event-overlay area past BATTLE.BIN:
 * battle_menu_build_sprite_page lays it out as two sprite pages and the
 * scrolling ability list saves its five 16-colour CLUTs there. WORLD keeps
 * its own g_world_menu_panel_buffer at the same address. */
typedef union battle_menu_scratch_buffer {
    menu_sprite_page_t sprite_pages[2];
    u16 clut_save[5 * 16];
} battle_menu_scratch_buffer_t;

/* The u16 x/y translation the status threads add to the frame tiles and
 * lines, read from the head of each overlay's panel-origin table (ATTACK
 * g_attack_character_status_frame_rect, DEBUGCHR
 * g_debugchr_character_status_frame_rect, EQUIP
 * g_equip_character_status_frame_rect, REQUIRE
 * g_require_character_status_frame_rect). Derived from the tile/line fixup
 * loops in the *_panel_run_character_status_thread routines. */
typedef struct battle_menu_status_panel_offset_pair {
    u16 x; /* 0x00 */
    u16 y; /* 0x02 */
} battle_menu_status_panel_offset_pair_t;

/* The numeric editor's two adjacent rectangles: the first also supplies the
 * texture-page origin to image loading, while the second places the portrait. */
typedef struct battle_menu_status_panel_numeric_geometry {
    union {
        RECT texture_rect;
        battle_image_location_t texture_origin;
    } source;            /* 0x00 */
    RECT draw_area_rect; /* 0x08 */
} battle_menu_status_panel_numeric_geometry_t;

/* Per-frame character status buffer (0x3d8 bytes), double buffered by the
 * *_panel_run_character_status_thread routines, which copy the first buffer
 * to the second (base + 0x3d8) before the render loop starts. Evidence, from
 * the DEBUGCHR routine and its byte-identical twins:
 *  - the DR_MODE packets at +0x00/+0x0c/+0x18 are submitted individually, and
 *    the numeric threads fill the same slots with
 *    battle_gfx_set_draw_mode_for_texture_page;
 *  - the submit loops walk the SPRTs from +0x24 with the sprite index as their
 *    counter (0..11 from +0x24, 12 at +0x114, 13 at +0x128, 14..18 from
 *    +0x13c, 19..23 from +0x1a0);
 *  - the TILE/LINE_F2 pair at +0x204/+0x224 is the one
 *    battle_menu_status_panel_menu_primitives_t describes;
 *  - the two draw-offset packets follow from the SetDrawOffset(packet,
 *    position) calls at +0x2a4/+0x2b0 and +0x2b4/+0x2c0;
 *  - +0x2c4 and +0x3b0 are the arguments of
 *    battle_menu_init_numeric_display_frame_primitives and the
 *    *_gfx_init_scaled_draw_area_packets / *_gfx_build_scaled_draw_area_packets
 *    pair. */
typedef struct battle_menu_status_panel_buffer {
    DR_MODE draw_modes[3];                                       /* 0x000 */
    SPRT sprites[24];                                            /* 0x024 */
    TILE tiles[2];                                               /* 0x204 */
    LINE_F2 lines[8];                                            /* 0x224 */
    battle_menu_status_panel_draw_offset_t draw_offsets[2];      /* 0x2a4 */
    world_menu_palette_primitives_t numeric_frame;               /* 0x2c4 */
    battle_menu_status_panel_portrait_primitive_tail_t portrait; /* 0x3b0 */
} battle_menu_status_panel_buffer_t;
typedef char
    battle_menu_status_panel_buffer_size_must_be_0x3d8[(sizeof(battle_menu_status_panel_buffer_t) == 0x3d8) ? 1 : -1];

/* Editor storage is addressed both as a status record and as 28 halfwords. */
typedef union battle_menu_status_panel_slot_storage {
    battle_menu_status_panel_slot_state_t state;
    s16 halfwords[0x1c];
} battle_menu_status_panel_slot_storage_t;
typedef char battle_menu_status_panel_slot_storage_size_must_be_0x38
    [(sizeof(battle_menu_status_panel_slot_storage_t) == 0x38) ? 1 : -1];

/* Unit gauge record (0x1e bytes) copied from battle_stats_t for the status
 * panel; the same shape as the first 0x1e bytes of battle_unit_status_record_t. */
typedef struct battle_menu_status_panel_gauges {
    s16 level;      /* 0x00 */
    s16 team_state; /* 0x02 */
    s16 _04;
    s16 _06;
    s16 experience; /* 0x08 */
    s16 unit_index; /* 0x0a */
    u16 hp;         /* 0x0c */
    s16 _0e;
    u16 max_hp; /* 0x10 */
    u16 mp;     /* 0x12 */
    s16 _14;
    u16 max_mp; /* 0x16 */
    s16 ct;     /* 0x18 */
    s16 _1a;
    s16 max_ct; /* 0x1c */
} battle_menu_status_panel_gauges_t;
typedef char
    battle_menu_status_panel_gauges_size_must_be_0x1e[(sizeof(battle_menu_status_panel_gauges_t) == 0x1e) ? 1 : -1];

/* One value bar: the Gouraud bar is drawn value/limit wide. */
typedef struct battle_menu_status_panel_bar {
    s16 value;
    s16 _unused_02;
    s16 limit; /* 0x04; a zero limit draws the fixed 4-pixel stub */
} battle_menu_status_panel_bar_t;
typedef char battle_menu_status_panel_bar_size_must_be_6[(sizeof(battle_menu_status_panel_bar_t) == 6) ? 1 : -1];

/* Editor state record (0x24 bytes) of the editor display threads. DEBUGCHR's
 * two instances, g_debugchr_panel_selected_billboard and
 * g_debugchr_panel_comparison_billboard, are 0x24 apart. `mode` selects
 * one of the 0xc-byte geometry rows and the panel's CLUT; `bars` holds the
 * three value/limit triples the Gouraud bars are drawn from. */
typedef struct battle_menu_status_panel_editor_state {
    u8 level;       /* 0x00; debugchr_editor_apply_values_to_battle_unit writes it back */
    u8 _padding_01; /* aligns mode */
    s16 mode;       /* 0x02 */
    s16 list_index; /* 0x04: AT-list position; drawn as dashes (format 0xc00)
                       when negative */
    s16 _unused_06;
    u8 experience;                          /* 0x08; written back with level */
    u8 _padding_09;                         /* aligns unit */
    s16 unit;                               /* 0x0a */
    battle_menu_status_panel_bar_t bars[3]; /* 0x0c */
    s16 _unused_1e[3];
} battle_menu_status_panel_editor_state_t;
typedef char battle_menu_status_panel_editor_state_size_must_be_0x24
    [(sizeof(battle_menu_status_panel_editor_state_t) == 0x24) ? 1 : -1];

/* One numeric-display entry (0xc bytes). battle_menu_draw_numeric_display_entries
 * takes a base and a count of these; the display threads write only the
 * halfword at +8, choosing the code the value is drawn with. */
typedef struct battle_menu_status_panel_numeric_entry {
    u8 _unused_00[8];
    s16 format; /* 0x08: digits argument; low byte digit count, high byte sign/marker
                   flags (0xc00/0xe00 draw dashes) */
    u8 _unused_0a[2];
} battle_menu_status_panel_numeric_entry_t;
typedef char battle_menu_status_panel_numeric_entry_size_must_be_0xc
    [(sizeof(battle_menu_status_panel_numeric_entry_t) == 0xc) ? 1 : -1];

/* Character-editor packet block (0x30c bytes) built by the editor display
 * threads. Two copies sit back to back and `frame & 1` selects one, so the
 * copy stride proves the size. The head is the numeric display frame that
 * battle_menu_init_numeric_display_frame_primitives builds, the SPRT/POLY
 * strides come from the walks that configure and submit them, and the
 * draw-offset packets from the SetDrawOffset pairs. */
typedef struct battle_menu_status_panel_editor_packet {
    world_menu_palette_primitives_t numeric_frame;        /* 0x000 */
    DR_MODE draw_mode_a;                                  /* 0x0ec */
    DR_MODE draw_mode_b;                                  /* 0x0f8 */
    SPRT value_sprites[4];                                /* 0x104 */
    SPRT label_sprites[7];                                /* 0x154; [6] at 0x1cc is the mode row */
    POLY_G4 bars[3];                                      /* 0x1e0 */
    POLY_FT4 portrait[4];                                 /* 0x24c */
    battle_menu_status_panel_draw_offset_t draw_offset_a; /* 0x2ec */
    battle_menu_status_panel_draw_offset_t draw_offset_b; /* 0x2fc */
} battle_menu_status_panel_editor_packet_t;
typedef char battle_menu_status_panel_editor_packet_size_must_be_0x30c
    [(sizeof(battle_menu_status_panel_editor_packet_t) == 0x30c) ? 1 : -1];

/* Status-panel packet block (0x1d8 bytes), doubled the same way as the editor
 * block. sprites[6] doubles as the scroll cursor: the display threads write
 * its texture window from the scroll row/column. */
typedef struct battle_menu_status_panel_packet {
    world_menu_palette_primitives_t numeric_frame;        /* 0x000 */
    SPRT sprites[7];                                      /* 0x0ec */
    DR_MODE draw_mode_a;                                  /* 0x178 */
    DR_MODE draw_mode_b;                                  /* 0x184 */
    battle_menu_status_panel_draw_offset_t draw_offset_a; /* 0x190 */
    battle_menu_status_panel_draw_offset_t draw_offset_b; /* 0x1a0 */
    POLY_FT4 portrait;                                    /* 0x1b0 */
} battle_menu_status_panel_packet_t;
typedef char
    battle_menu_status_panel_packet_size_must_be_0x1d8[(sizeof(battle_menu_status_panel_packet_t) == 0x1d8) ? 1 : -1];

/* Parameter block of the editor display threads, reached through the
 * 0x400-byte battle thread table. The shake offset is written as a word from
 * the s16 shake tables and read back as its low halfword, which is how the
 * target loads it into the draw-offset y. Only the members the display threads
 * use are named; the record's total size is not proven. */
typedef struct battle_menu_status_panel_display_thread {
    u16 x;           /* 0x00 */
    u16 _padding_02; /* aligns shake_y */
    union {
        s32 word; /* 0x04 */
        u16 low;
    } shake_y;
    s32 flags;     /* 0x08: 0x20 and 0x40 start the two shake tables, 0x80
                      suppresses all drawing */
    void* work;    /* 0x0c: redraw request, cleared once serviced */
    s32 highlight; /* 0x10: nonzero forces the alternate (highlight) CLUTs; bound as
                      g_bunit_status_display_highlight and the WORLD display globals */
} battle_menu_status_panel_display_thread_t;

/* Item graphic load descriptor filled by *_gfx_build_item_graphic_descriptor. */
typedef struct battle_menu_status_panel_graphic_descriptor {
    s16 x_load_location;
    s16 y_load_location;
    s16 width;
    s16 height;
    u16 clut;
    s16 tpage;
} battle_menu_status_panel_graphic_descriptor_t;
typedef char battle_menu_status_panel_graphic_descriptor_size_must_be_0xc
    [(sizeof(battle_menu_status_panel_graphic_descriptor_t) == 0xc) ? 1 : -1];

/* Ordering-tag/intensity/scale record read by the
 * *_gfx_build_scaled_sprite_primitive_chain builders. */
typedef struct battle_menu_status_panel_scaled_sprite {
    u32* ordering_tag;
    u8 intensity;
    u8 _padding_05[3]; /* aligns link_primitive */
    s32 link_primitive;
    s16 scale_x;
    s16 scale_y;
    u8 _unused_10[8];
    s16 offset[6];
} battle_menu_status_panel_scaled_sprite_t;
typedef char battle_menu_status_panel_scaled_sprite_size_must_be_0x24
    [(sizeof(battle_menu_status_panel_scaled_sprite_t) == 0x24) ? 1 : -1];

/* Glyph source rectangle used by the *_text_render_decimal_value renderers. */
typedef struct battle_menu_status_panel_glyph {
    s16 source_x;
    s16 source_y;
    s16 width;
    s16 height;
} battle_menu_status_panel_glyph_t;

/* One gauge value for the *_text_render_decimal_entry_list and
 * *_text_render_signed_decimal_entries renderers: the text position, a
 * pointer to the value and the decimal-format flags. */
typedef struct battle_menu_status_panel_gauge_entry {
    u16 x;
    u16 y;
    s16* value;
    s16 flags;
    u16 _padding_0a; /* tail padding to 4-byte alignment */
} battle_menu_status_panel_gauge_entry_t;
typedef char
    battle_menu_status_panel_gauge_entry_size_must_be_0xc[(sizeof(battle_menu_status_panel_gauge_entry_t) == 0xc) ? 1
                                                                                                                  : -1];

/* Text cursor passed to *_text_render_decimal_value. The entry-list renderers
 * set the word at +0x0c to a 4bpp colour fill pattern (0x77777777 or
 * 0x88888888 for negative values, 0xbbbbbbbb or 0xcccccccc for positive ones)
 * and clear it after each value; the value renderer clears it while it draws
 * the sign glyphs. */
typedef struct battle_menu_status_panel_text_position {
    u16 x;
    u16 y;
    u8 _unused_04[4];
    u16 row_stride;  /* 0x08 */
    u16 _padding_0a; /* aligns color */
    u32 color;       /* 0x0c */
} battle_menu_status_panel_text_position_t;
typedef char battle_menu_status_panel_text_position_size_must_be_0x10
    [(sizeof(battle_menu_status_panel_text_position_t) == 0x10) ? 1 : -1];

/* Provisional 0x7c-byte menu window record built by this routine: two draw
 * modes, four sprites (background, left cap, right cap and a style icon) and
 * three optional extra sprites for the compound styles. */
typedef struct battle_menu_window_record {
    DR_MODE mode0;   /* 0x00 */
    DR_MODE mode1;   /* 0x0c */
    SPRT sprites[4]; /* 0x18 */
    SPRT* extra[3];  /* 0x68 */
    s32 _unknown_74; /* 0x74 */
    s32 _unknown_78; /* 0x78 */
} battle_menu_window_record_t;

/* Provisional window request: VRAM source point, size, screen point and the
 * load parameters for the window background quad. */
typedef struct battle_menu_window_spec {
    battle_image_location_t load;             /* 0x00 */
    u16 width;                                /* 0x04 */
    u16 height;                               /* 0x06 */
    battle_image_location_t screen;           /* 0x08 */
    u8 _unused_0c[4];                         /* 0x0c */
    world_gfx_image_load_parameters_t params; /* 0x10 */
    u8 _unused_1c[0x10];                      /* 0x1c */
    s16 style;                                /* 0x2c */
} battle_menu_window_spec_t;

/* Provisional menu text image record rendered by
 * battle_menu_render_text_image_at_record_origin: a text image buffer is
 * built (the wide builder when box_type is 0x10), the text at +0x10 is
 * drawn from the origin at +0x40, and the RECT at +0x08 is uploaded. Same
 * layout as the WORLD world_menu_text_image_t and the text threads' dialog
 * records. */
typedef struct battle_menu_text_image {
    void* buffer; /* 0x00 */
    u16 width;    /* 0x04 */
    u16 height;   /* 0x06 */
    RECT rect;    /* 0x08 */
    void* text;   /* 0x10 */
    u8 _unused_14[2];
    u16 dialog_type; /* 0x16; window-image mode */
    u16 box_type;    /* 0x18: 0 skips rendering, 0x10 selects the wide builder */
    u8 _unused_1a[8];
    u16 first_line; /* 0x22 */
    u16 last_line;  /* 0x24 */
    u8 _unused_26[0x30 - 0x26];
    s16 tail_offset; /* 0x30; arrow position */
    u8 _unused_32[0x40 - 0x32];
    u16 origin_x; /* 0x40 */
    u16 origin_y; /* 0x42 */
} battle_menu_text_image_t;
typedef char battle_menu_text_image_size_must_be_0x44[(sizeof(battle_menu_text_image_t) == 0x44) ? 1 : -1];

/* Provisional 8-byte header copied out of the request for the caller. */
typedef struct battle_menu_window_header {
    u16 texture_x;  /* 0x00 */
    u16 texture_y;  /* 0x02 */
    s16 half_width; /* 0x04 */
    u16 height;     /* 0x06 */
} battle_menu_window_header_t;

/* Origin and row geometry used by the EVENT menu text image renderers. */
typedef struct battle_menu_text_image_bounds {
    u16 x;
    u16 y;
    u16 row_stride;
    s16 line_height;
} battle_menu_text_image_bounds_t;

/* Pen position (not a rectangle) handed to the number glyph renderers;
 * callers point it at menu_text_state_t.origin_x. */
typedef struct battle_rect {
    s16 x; /* 0x00 */
    s16 y; /* 0x02 */
} battle_rect_t;

/* The BATTLE twin of the block above, at 0x80165f9c: same layout, but its
 * first word is not the script-variable pointer.  Named separately so the
 * BATTLE object does not have to borrow the WORLD record's type. */
typedef struct menu_text_state {
    s32 _unused_00; /* 0x00 (0x80165f9c) */
    s16 origin_x;   /* 0x04 (0x80165fa0): battle_menu_set_text_origin */
    s16 origin_y;   /* 0x06 (0x80165fa2) */
    s32 _unused_08; /* 0x08 (0x80165fa4) */
    s32 stride;     /* 0x0c (0x80165fa8): g_menu_inner_window_width */
    s32 color;      /* 0x10 (0x80165fac): glyph blit fill; 0x44444444 while text colour 4 is drawn */
} menu_text_state_t;

/* One cursor-navigation node of the HELPMENU, BUNIT, ATTACK deployment and
 * DEBUGCHR editor screens (WORLD's world_help_navigation_entry_t has the same
 * layout): the value or text id it refers to, its help-text base, the node
 * reached for each d-pad direction (up 0x1000, down 0x4000, right 0x2000,
 * left 0x8000) and the cursor position. */
typedef struct help_navigation_record {
    s16* value;          /* 0x00 */
    s16 help_base;       /* 0x04 */
    u8 destination[4];   /* 0x06: up, down, right, left */
    s16 x;               /* 0x0a */
    s16 y;               /* 0x0c */
    s16 vertical_cursor; /* 0x0e: nonzero: the cursor points down and bobs along y */
} help_navigation_record_t;
typedef char help_navigation_record_size_must_be_0x10[(sizeof(help_navigation_record_t) == 0x10) ? 1 : -1];

/* Provisional: text pen position and right margin. The caller passes
 * &g_menu_text_state.origin_x, whose stride sits at +0x08. */
typedef struct battle_text_pen {
    u16 x;           /* 0x00 */
    u16 y;           /* 0x02 */
    s32 _unused_04;  /* 0x04 */
    s32 right_limit; /* 0x08 */
} battle_text_pen_t;

extern battle_menu_allocation_t g_battle_menu_allocator_slot_records[16];
extern u8 g_battle_menu_buffer_arena[53][0x100];
extern u8 g_battle_menu_buffer_allocation_map[53];
extern s32 g_battle_menu_companion_executable_loading;
extern s32 g_battle_menu_companion_executable_request_index;
extern s32 g_battle_menu_current_id;
extern s32 g_battle_menu_help_open;
extern s32 g_battle_menu_help_opening;
extern s16 g_battle_menu_hovered_unit_display_mode;
extern RECT g_battle_menu_hovered_unit_stats_display;
extern world_menu_entry_t g_battle_menu_idle_action_entries[];
extern u16 g_battle_menu_input_disabled;
extern u8 g_battle_menu_numeric_display_frame_offsets[12][8];
extern u8 g_battle_menu_numeric_display_palette_indices[12];
extern RECT g_battle_menu_numeric_display_texture_window;
extern s32 g_battle_menu_packet_buffer_index;
extern u8 g_battle_menu_palette_colors[];
extern s32 g_battle_menu_panel_fade_intensity;
extern s32 g_battle_menu_panel_fade_mode;
extern RECT g_battle_menu_portrait_palette_rect;
extern RECT g_battle_menu_portrait_pixel_rect;
extern u8 g_battle_menu_restore_pending;
extern s16 g_battle_menu_right_unit_display_mode;
extern RECT g_battle_menu_right_unit_stats_display;
extern s32 g_battle_menu_secondary_companion_executable_request_index;
extern s32 g_battle_menu_selected_skill_target;
extern s32 g_battle_menu_slot_owner_thread_ids[3];
extern s32 g_battle_menu_slot_states[3];
extern s32 g_battle_menu_status_requested;
extern s32 g_battle_menu_status_screen_selected;
extern u16 g_battle_menu_transition_state;
extern battle_menu_record_t g_battle_menu_unit_selection_records[BATTLE_UNIT_SLOT_COUNT];
extern menu_text_state_t g_menu_text_state;
extern battle_unit_status_record_t g_battle_menu_active_turn_banner;

/* 0x40-byte billboard block at 0x8014d0a0 saved and restored whole with the AT
 * banner: unit stats indexed as halfwords by
 * battle_menu_store_units_small_in_battle_display_data, with the editor panel
 * view g_battle_unit_editor_panel_data at +0xe. */
extern s16 g_battle_menu_billboard_data[0x20];
extern u8 g_battle_menu_billboard_skillset_name[];
extern u8 g_battle_menu_billboard_secondary_skillset_name[];
extern u8 g_battle_menu_billboard_unit_name[];
extern u8 g_battle_menu_billboard_job_name[];
extern u16 g_battle_menu_ability_display_flags[20];
extern s32 g_battle_menu_used_skillset_id;
extern s32 g_battle_menu_used_item_id;
extern s32 g_battle_menu_system_function_command;
extern s32 g_battle_menu_system_function_option;
extern u8 g_battle_menu_restore_depth;
extern u8 g_battle_menu_restore_unit_id;
extern battle_menu_id_record_t g_battle_menu_id_records[31];
extern u8 g_battle_menu_glyph_image_data[];
extern u8 g_battle_menu_default_palette_colors[];
extern RECT g_battle_menu_disabled_texture_window;
extern s32 g_battle_menu_scroll_list_depth;
extern s16* g_battle_menu_ability_display_flags_ptr;
extern u16 g_battle_menu_status_visible_rows;
extern u16 g_battle_menu_status_scroll_rows;

/* Row actions of the auto-battle setting menu (menu entry 0x80166b4c, +0x24). */
extern s16 g_battle_menu_auto_battle_row_actions[8];
extern s16 g_battle_menu_action_slot_row_actions;
extern s16 g_battle_menu_wait_row_actions[4];
extern u16 g_battle_menu_status_list_row_actions[8];
extern u16 g_battle_menu_system_function_row_actions[];
extern s16 g_battle_menu_confirm_stored_unit_row_actions;
extern s16 g_battle_menu_set_auto_battle_row_actions;
extern u16* g_battle_menu_window_tween_easing_tables[];
extern u16 g_battle_menu_hovered_stats_frame_count;
extern RECT g_battle_menu_sprite_page_texture_windows[5];
extern world_gfx_image_load_parameters_t g_battle_menu_sprite_page_image_params[30];
extern RECT g_battle_menu_attack_caster_stats_display;
extern RECT g_battle_menu_attack_target_stats_display;
extern u8 g_battle_menu_attack_caster_stats_setup[];
extern u8 g_battle_menu_attack_target_stats_setup[];
extern s32 g_battle_menu_companion_executable_lbas[];
extern s32 g_battle_menu_companion_executable_sizes[];
extern s32 g_battle_menu_companion_executable_offsets[];
extern s32 g_battle_menu_secondary_companion_executable_lbas[];
extern s32 g_battle_menu_secondary_companion_executable_sizes[];
extern s32 g_battle_menu_secondary_companion_loading;
extern u8 g_battle_menu_idle_action_entry_labels[];
extern s32 g_battle_menu_building_thread_started;
extern u8 g_battle_menu_panel_fade_modes[];
extern u8 g_battle_menu_idle_action_flag_clear_ids[];
extern s16 g_battle_menu_status_left_text_ids[20];
extern s16 g_battle_menu_status_right_text_ids[20];
extern s32 g_battle_menu_action_menu_build_result;
extern s16 g_battle_menu_selected_option;
extern battle_action_data_t* g_battle_menu_preview_target_action;
extern world_fade_tile_frame_t g_battle_menu_panel_fade_frames[];
extern battle_menu_scratch_buffer_t g_battle_menu_scratch_buffer;
extern s32 g_battle_help_text_id_tables_ptr;
extern u16 g_battle_menu_anything_ability_id;
extern s16 g_battle_menu_action_slot_selected_option;
extern s32 g_battle_active_menu; /* active menu */
extern RECT g_battle_menu_ability_list_clut_rect;
extern s16 g_battle_menu_ability_scroll_offset;
extern s16 g_battle_menu_action_type_modes[][2];
extern void* g_battle_menu_buffer_slots[6];
extern s16 g_battle_menu_height_display_suppressed;
extern s32 g_battle_menu_hide_numeric_values; /* non-zero: draw digits as blanks (0x48, 0x10) */
extern world_gfx_image_load_parameters_t g_battle_menu_icon_strip_image_params[44];
extern u16 g_battle_menu_input_block_frames;
extern RECT g_battle_menu_number_glyph_rect;   /* glyph source rectangle in the number/sign font */
extern s16 g_battle_menu_pending_selection[4]; /* 4 shorts */
extern u8 g_battle_menu_quad_group_starts[];   /* first piece index per group; [n + 1] ends group n */
extern s32 g_battle_menu_resolved_command;
extern u8 g_battle_menu_restriction_message_ids[];
extern s16 g_battle_menu_result;            /* menu result: -4 pending, -1 cancelled */
extern s16 g_battle_menu_row_message_ids[]; /* per-row message id */

/* Action command staged by the battle menu and passed whole to
 * battle_action_preview_at_list; the WORLD twin is g_world_menu_preview_action. */
extern battle_ai_command_action_t g_battle_menu_selected_action;
extern world_gfx_image_load_parameters_t g_battle_menu_single_icon_image_params[5];
extern u8 g_battle_menu_skillset_disabled_flags[]; /* per-row greyed flag */
extern battle_unit_status_record_t g_battle_menu_status_billboard;
extern u8 g_battle_menu_text_origin_x[];
extern u8 g_battle_menu_text_origin_y[];

/* Twelve crop percentages, indexed by step. */
extern u16 g_battle_menu_zoom_percentages[];
extern s32 g_menu_text_param_22; /* 0x80166020: record +0x22 */
extern s32 g_menu_text_param_24; /* 0x80166024: record +0x24 */
extern battle_image_location_t g_battle_menu_texture_location;
extern u16 g_option_menu_open;
extern u16 g_option_menu_submenu_state;
extern u16 g_option_menu_transition_finished;
extern s32 g_battle_menu_status_enabled;

/* Source image used when battle overlays render menu glyphs. */
extern u8* g_battle_menu_glyph_image;
extern s32 g_battle_menu_text_section_offsets[];
extern world_menu_scroll_text_layout_t g_battle_menu_at_list_layout;
extern world_menu_quad_piece_t g_battle_menu_quad_pieces[]; /* quad piece table */
extern world_menu_quad_colors_t g_battle_menu_quad_colors;  /* vertex colours */
extern world_menu_quad_origin_t g_battle_menu_quad_origin;  /* screen origin */
extern battle_text_pen_t g_battle_menu_text_pen;
extern world_unit_editor_frame_t g_battle_menu_unit_editor_frames[2];

/* Full record bases, not the legacy aliases biased to thread_id. */
extern menu_window_buffer_t g_battle_menu_window_buffers[6]; /* 0x8016e43c */
extern s32 g_menu_inner_window_width;
extern s32 g_menu_text_palette_offset;

/* BATTLE's current menu-entry table pointer at 0x80173cb8 (menu threads are
 * started on &table[i]): battle_menu_init_subsystem_pointers points it at
 * g_battle_menu_idle_action_entries, and the event overlays that run over
 * BATTLE swap in their own tables and restore it. WORLD's twin is
 * g_world_menu_thread_menu_data. */
extern world_menu_entry_t* g_battle_menu_thread_menu_data;
extern void* g_menu_text_pointer;

void battle_menu_display_hovered_unit_stats(
    RECT* frame_rect, s32 mode, menu_number_entry_t* entries, battle_unit_status_record_t* status);

void battle_handle_menu_cancel_input(void* menu);
void* battle_menu_alloc_buffer(s32 bytes);
void* battle_menu_alloc_memory(s32 size);
void* battle_menu_build_and_upload_window_frame_image(s32 width, s32 height, RECT* rect, s32 mode);
void battle_menu_build_idle_action_menu(s32 menu_id);
s32 battle_menu_build_quad_pieces(s32 group, POLY_GT4* poly);
void battle_menu_build_skillset_entries(void);
void battle_menu_build_unit_status_list(void);
void battle_menu_build_window_image_row(const u16* tiles, u16* row, s32 count, s32 index);
void battle_menu_building_thread(void);
s32 battle_menu_can_accept_input(void);
void battle_menu_check_action_menu_restrictions(void);
void battle_menu_clear_idle_action_menu_entry_flags(void);
void battle_menu_close_move_help(void);
void battle_menu_dispatch_idle_action_menu(void);
void battle_menu_display_triangle_selection(void);
void battle_menu_draw_text_columns_narrow(world_menu_entry_t* entry, s32* row_offset, void* buffer);
void battle_menu_enter_status_screen_selection(void);
void battle_menu_fade_out_thread(void);
void battle_menu_free_high_overlay(void);
s32 battle_menu_get_cursor_bob_offset(s32 mode);
s32* battle_menu_get_selected_ability_address(void);
void battle_menu_icon_linked_entry_thread(void);
s32 battle_menu_init_action_menu(battle_unit_misc_data_t* unit);
void battle_menu_init_buffer_allocator(void);
s32 battle_menu_init_monster_skill_check(battle_stats_t* unit);
s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height);
void battle_menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut);
s32 battle_menu_get_id_based_on_mount_moveable_actable(s32 misc_id);
void battle_menu_init_subsystem_pointers(void);
void battle_menu_init_subsystems(void);
s32 battle_menu_init_system_function(s32 type, s32 b, s32 c, s32 d, s32 e);
s32 battle_menu_is_still_building(void);
s32 battle_menu_load_elemental_abilities(s32 skillset_slot, u8 unit_id);
void battle_menu_leave_status_screen_selection(void);
s32 battle_menu_lerp_fixed12(s32 start, s32 end, s32 factor);
void battle_menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, RECT* from, RECT* to, s32 factor);
void battle_menu_open_active_unit_idle_action_menu(void);
void battle_menu_open_companion_executable(void);
void battle_menu_open_companion_executable_6(void);
void battle_menu_open_debugchr_unit_editor(void);
void battle_menu_open_free_cursor_help(void);
void battle_menu_open_free_cursor_mini_menu(void);
void battle_menu_open_mini_menu(void);
void battle_menu_open_move_confirm(void);
void battle_menu_open_non_active_unit_idle_action_menu(void);
void battle_menu_open_option_menu_2(void);
void battle_menu_option_menu_thread(void);
void battle_menu_refresh_hovered_unit_stats_display(void);
void battle_menu_request_companion_executable_8(void);
void battle_menu_request_open_companion_executable(s32 request_index);
void battle_menu_request_open_secondary_companion_executable(s32 request_index);
void battle_menu_reset_buffer_slots(void);
void battle_menu_reset_unit_record(s32 record_index);
void battle_menu_retry_alloc_with_message(u32 byte_length);
void battle_menu_reset_subsystems(void);
void battle_menu_run_numeric_display_panel_thread(void);
void battle_menu_run_skillset_thread(void);
void battle_menu_run_system_function_thread(void);
void battle_menu_selected_tile_info_display_thread(void);
void battle_menu_set_next_script_action_menus(void);
void battle_menu_set_option_transition_finished(void);
void battle_menu_start_building_thread(void);
void battle_menu_start_entry_thread(void);
void battle_menu_start_mini_menu_display_thread(void);
void battle_menu_start_system_function_thread(s32 system_function);
void battle_menu_store_units_small_in_battle_display_data(void);
void battle_menu_update_panel_fade(void);
void battle_menu_draw_active_window_frames(void);
void battle_menu_build_zoom_draw_area_pair(void* packets, void* geometry, s32 step, s32 flag);
void battle_menu_build_ability_preview_at_list(void);
void battle_menu_run_scrolling_ability_list_thread(void);
void battle_menu_select_unit_action_slots_thread(void);
s32 battle_menu_resolve_selection(void);
s32 battle_menu_get_unit_action_menu_id(battle_stats_t* unit);
s32 battle_menu_load_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids);

s32 battle_menu_get_unit_skillset_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out);

s32 battle_menu_collect_throwable_items(s32 unit_id, u8 ability, u8* items);
void battle_menu_configure_frame_cluts(struct battle_menu_window_record* frame);
void battle_clear_menu_render_buffer(void* buffer, s32 bytes);
void* battle_menu_alloc_window_buffer_pair(void);
void battle_menu_build_ability_list(s32 mode);
void battle_menu_build_and_upload_window_image(s32 width, s32 height, s32 rect, s32 mode, s32 tail_offset);
void battle_menu_build_sprite_page(s32 page_index, void* base_screen);
s32 battle_menu_build_unit_action_menus(s32 unit_id, u8* entries, u8* empty, u8* menu_types);
void battle_menu_build_unit_portrait_poly(POLY_FT4* poly, u32 battle_id);
void* battle_menu_build_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset, s32 keep);
s32 battle_menu_call_load_charge_skillset(s32 unit_id, s32 skillset_id, s16* ability_ids);
void battle_menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit);
void battle_menu_clear_status_menu_state(void);
s32 battle_menu_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out);
s32 battle_menu_collect_math_skill_abilities_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask);
s32 battle_menu_collect_monster_skill_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out);
void battle_menu_confirm_action_silently(void* menu);
s32 battle_menu_copy_ff_terminated_bytes_to_halfwords(s16* dst, u8* src);
void battle_menu_copy_palette_colors(const void* source);
void battle_menu_copy_unit_data_to_status_billboard(battle_stats_t* unit, battle_unit_status_record_t* output);

s32 battle_menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, battle_ai_command_action_t* action);

s32 battle_menu_display_item_inventory_ability(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 battle_menu_load_math_skill_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids);

void battle_menu_draw_numeric_display_entries(
    s32 buffer, menu_number_entry_t* entries, menu_number_position_t* position, s32 count);

void battle_menu_free_buffer(s32 buffer);
s32 battle_menu_get_dead_unit_selection(s32 battle_id);
s32 battle_menu_has_status_effect_for_status_window(s32 unit_id);
s32 battle_menu_init_attack_resources_and_threads(void);
void battle_menu_init_numeric_display_frame_primitives(RECT* bounds, world_menu_palette_primitives_t* menu);
void battle_menu_init_primitive_colors_palette_bank_0(world_menu_palette_primitives_t* menu);
void battle_menu_init_primitive_colors_palette_bank_1(world_menu_palette_primitives_t* menu);
void battle_menu_init_semitransparent_sprt(SPRT* sprite);
void battle_menu_run_icon_selection_loop(void);
void battle_menu_select_sprite_clut(SPRT* sprite);
void battle_menu_set_disabled_texture_window(u8* menu);
void battle_menu_submit_frame_primitives(struct battle_menu_window_record* frame);
void battle_menu_update_selection_from_input(battle_menu_idle_action_entry_t* ctx, s32* sel);

void battle_update_menu_cursor_primitives(
    struct world_menu_icon_thread_param* param, struct world_menu_icon_sprites* record, s32 frame, s32 cursor);

s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns);
s32 battle_menu_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 battle_menu_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 battle_menu_load_math_skill_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids);
void battle_menu_load_palette_and_text_pointers(void);
s32 battle_menu_load_skillset(s32 unit_id, u8 skillset, u8 menu);
s32 battle_menu_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
void battle_menu_open_help(void);
void battle_menu_open_mini_menu_help(void);
void battle_menu_open_secondary_companion_executable(void);
void battle_menu_preview_ability_turn_at_list(void);
void battle_menu_release_current_thread_slot(void);
void battle_menu_restore_selection_for_selected_unit(s32 menu_id);
void battle_menu_run_companion_overlay_9_10_thread(void);
void battle_menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id);
void battle_menu_submit_numeric_display_frame_primitives(world_menu_palette_primitives_t* display);
void battle_menu_widen_bytes_to_halfwords(s16* dst, u8* src, s32 count);
void battle_menu_zoom_cursor_frame(const void* source, SPRT* sprite, s32 step);
void battle_set_menu_sprite_clut_variant_1(SPRT* sprite);
void battle_menu_open_illegal_move_help(void);
s32 battle_menu_draw_selection_data(s32 otag, s32 controller_input);
void battle_menu_render_text_image_at_record_origin(battle_menu_text_image_t* record);

void battle_menu_build_window_sprites(
    battle_menu_window_header_t* header, battle_menu_window_spec_t* spec, battle_menu_window_record_t* record);

s32 battle_menu_display_projected_action_effect(POLY_FT4* prim, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, world_gfx_image_load_parameters_t* params);

void battle_menu_clear_option_menu_open(void);
void battle_menu_open_bunit(void);
void battle_menu_run_bunit_transition_thread(void);
void battle_menu_set_option_menu_open(void);
void battle_draw_menu_number_glyphs(s32 value, s32 digits, void* resource, battle_rect_t* pos);
void battle_menu_display_text(u32 text_id, s32 image, battle_text_pen_t* origin, u8* text);

u8* battle_menu_redraw_text_page_on_scroll_in_rect(
    world_menu_entry_t* entry, s32* row_offset, s32* redraw, s32 unused, RECT* area);

void battle_menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table);
void battle_menu_animate_window_quad_crop(menu_window_buffer_t* buffers, RECT* rect);
void battle_menu_free_memory(void* buffer);
void battle_menu_display_text_entry(s32 text_id, void* image, void* origin);
void battle_menu_handle_action(void* menu, s32 selected_option);
void battle_menu_set_text_origin(s16 x, s16 y);
s32 battle_menu_should_close_thread(s32* out_flag);
void battle_menu_store_unit_names_and_event_block_data(s32 event_id, s32 unit_a, s32 unit_b);

/* text */
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
    BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK = 0x1ff,
    BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL = 0x200,
} battle_numeric_display_selector_e;

/* Glyph state of the dialogue text renderer at 0x8016dad4, filled by
 * battle_text_character_handling_thread and drawn by battle_text_draw_dialogue_glyph. */
typedef struct battle_text_glyph_state {
    u16 flags_0;         /* 0x00; bits 0-1 give the glyph's pixel phase */
    u16 _unused_02;      /* 0x02 */
    u16 character_4;     /* 0x04 */
    u8 _unused_06[6];    /* 0x06 */
    s16 dialogue_type_c; /* 0x0c */
    s16 palette_e;       /* 0x0e */
} battle_text_glyph_state_t;
typedef char battle_text_glyph_state_size_must_be_0x10[(sizeof(battle_text_glyph_state_t) == 0x10) ? 1 : -1];

/* Text variables $27 and $28 (rows of glyph pixels drawn, 0x10 by default),
 * bound separately: battle_menu_display_text also takes the table base, and
 * element spellings let GCC address them from it. */
extern s32 g_battle_text_substitution_value_27;
extern s32 g_battle_text_substitution_value_28;
extern char g_battle_text_wait_for_allocation_message[];
extern char g_battle_text_wait_for_file_allocation_message[];
extern u8 g_battle_text_glyph_bitmaps[];
extern s16 g_battle_text_speaker_swap_unit_a;
extern s16 g_battle_text_speaker_swap_unit_b;
extern RECT g_battle_numeric_display_frame_rect;
extern RECT g_battle_numeric_display_texture_window;
extern RECT g_battle_numeric_display_text_rect_a;
extern RECT g_battle_numeric_display_text_rect_b;
extern RECT g_battle_numeric_display_text_rect_c;
extern world_gfx_image_load_parameters_t g_battle_numeric_display_image_params[18];
extern u8 g_battle_numeric_display_entries[];
extern world_menu_palette_slot_t g_battle_numeric_display_palettes[];
extern u8 g_battle_player_unit_fallen;

/* Player name buffer substituted for text command 0xE0, 0xFE-terminated;
 * 0x801720cf is its last byte. */
extern u8 g_battle_player_name[20];
extern RECT g_battle_text_large_number_glyph_rect; /* glyph source rectangle in the large number font */

extern menu_text_state_t
    g_battle_text_typewriter_state; /* origin/stride passed to battle_text_render_glyph_to_4bpp_image */

extern RECT g_battle_text_typewriter_vram_rect; /* VRAM destination of the finished glyph */
extern const u8* g_text_glyph_bitmap_data;

/* Volatile views of g_battle_text_section_pointers[11] and [22] for
 * attack_deploy_find_fieldable_units, which matches only with volatile
 * accesses. */
extern u8* volatile g_battle_text_pointer_11;
extern u8* volatile g_battle_text_pointer_22;
extern battle_text_glyph_state_t g_battle_text_typewriter_glyph;
extern const char g_battle_text_decimal_format[];
extern u16 g_battle_text_message_duration_frames;
extern u8* g_battle_text_pointer_backup[32];
extern s32 g_battle_text_pointer_backup_active;

/* BATTLE text IDs use their upper five bits to select one of these sections.
 * Overlay loaders replace individual entries as their text becomes active. */
extern u8* g_battle_text_section_pointers[32];

/* Glyph pixel widths, 0xD0 entries per 0xDx prefix page. */
extern const u8 g_text_glyph_widths[];
extern s32 g_battle_text_awaiting_input;
extern s32 g_battle_text_current_line;
extern u32 g_battle_text_decimal_divisors[];
extern s32 g_battle_text_glyphs_per_wait;             /* glyphs drawn before the typewriter waits */
extern const char g_battle_text_heap_dp_format[];     /* "DP %x\n" */
extern const char g_battle_text_heap_remain_format[]; /* "HEAP REMAIN %x MAX %x\n" */
extern const char g_battle_text_heap_task_format[];   /* "TASK %d %x\n" */

/* Pixel width of each line measured by the last call. */
extern s32 g_battle_text_line_widths[];
extern void* g_battle_text_message_buffer;
extern const char g_battle_text_min_heap_format[];           /* "MIN HEAP %x " */
extern char g_battle_text_reading_character_message[];       /* "reading charactor%d \n" */
extern u8 g_battle_text_slot_row_pitches[];                  /* per-slot source row pitch in bytes */
extern u8 g_battle_text_slot_source_columns[];               /* per-slot source column */
extern s32 g_battle_text_substitution_values[0x80];          /* text variables $00-$7F */
extern u8 g_battle_text_typewriter_column_scratch[14];       /* one-column scratch used by the phase shift */
extern u8 g_battle_text_typewriter_glyph_images[3][8][0x54]; /* eight glyph images per slot */
extern u8 g_battle_text_typewriter_previous_images[3][0x54]; /* previous image per slot */
void battle_text_run_unit_message_thread(void);
const u8* battle_find_text_id_location(const u8* text_section, s32 entry_id);
void battle_text_measure_pixels(s16* width, s16* rows, const u8* text);
void battle_text_render_glyph_to_4bpp_image(const u8* glyph_bitmap, s32 image, u16* origin, s32 palette_offset);
void battle_text_build_display_message(s32 offset, s32 flags, s32* out_width);
void battle_text_build_unit_name_list(void);
u8* battle_text_init_entry(u32 text_id);
void battle_text_init_menu_section_pointers(void);
void battle_text_set_message_duration_frames(u16 frames);
void battle_text_relocate_pointer_table(const u32* offsets);
void battle_text_restore_pointer_table(void);
void battle_text_save_pointer_table(void);
void battle_text_draw_large_number_glyphs(s32 value, s32 digits, void* resource, battle_rect_t* pos);
void battle_text_character_handling_thread(void);
void battle_text_configure_sprite_vram(RECT* rect, s16 width, s16 height, SPRT* sprite, s32 palette_row);
u8* battle_text_advance_cursor_with_separate_backreference_state(s32* remaining_bytes, u8* cursor, u8** return_cursor);
s32 battle_text_count_decimal_digits(s32 value);
void battle_text_draw_dialogue_glyph(s32 pen_x, s32 pen_y, s32 delay);
void battle_text_print_fnt_value_and_next(s32 value);
s32 battle_text_read_packed_nibble(u8* data, s32 index, s32 row, s32 stride);
s32 battle_text_resolve_format_string_id(s32 format_code, s32 value);

/* The caller leaves 0x16f in $a1, but the body never reads $a1. */
void battle_text_fill_field_with_terminators(u8* text);
void battle_text_mirror_packed_nibble_region_horizontal(u8* data, world_gfx_4bpp_rect_t* region);
u8* battle_text_find_next_character(world_text_backreference_state_t* reader, u8* p);

/* other */
/* Unshifted zodiac IDs. Party and battle records store these in the high
 * nibble; ordinary birthdays never produce Serpentarius. */
typedef enum zodiac_sign {
    ZODIAC_SIGN_ARIES = 0,
    ZODIAC_SIGN_TAURUS = 1,
    ZODIAC_SIGN_GEMINI = 2,
    ZODIAC_SIGN_CANCER = 3,
    ZODIAC_SIGN_LEO = 4,
    ZODIAC_SIGN_VIRGO = 5,
    ZODIAC_SIGN_LIBRA = 6,
    ZODIAC_SIGN_SCORPIO = 7,
    ZODIAC_SIGN_SAGITTARIUS = 8,
    ZODIAC_SIGN_CAPRICORN = 9,
    ZODIAC_SIGN_AQUARIUS = 10,
    ZODIAC_SIGN_PISCES = 11,
    ZODIAC_SIGN_SERPENTARIUS = 12,
} zodiac_sign_e;

/* The compatibility table is indexed by absolute zodiac-ID difference, not
 * by a particular sign. Value 3 is resolved from sex and monster flags. */
typedef enum zodiac_compatibility {
    ZODIAC_COMPATIBILITY_NEUTRAL = 0,
    ZODIAC_COMPATIBILITY_BAD = 1,
    ZODIAC_COMPATIBILITY_GOOD = 2,
    ZODIAC_COMPATIBILITY_GENDER_DEPENDENT = 3,
    ZODIAC_COMPATIBILITY_WORST = 4,
    ZODIAC_COMPATIBILITY_BEST = 5,
} zodiac_compatibility_e;

extern s32 g_battle_destination_camera_yaw;
extern u8 g_battle_job_level_gained_flag;
extern u8 g_battle_math_source_skillsets[4];
extern VECTOR g_battle_offset_screen_coords;
extern s16 g_battle_unique_value_list[];
extern s32 g_battle_unique_value_list_length;
extern u16 g_enemy_level_sum;
extern u8 g_highest_enemy_level;
extern s32 g_previous_battle_game_state;
extern s32 g_source_unit_misc_id;
extern s32 g_battle_saved_game_options;

/* Scratch map coordinates filled by battle_unit_copy_map_coords_from_misc_id;
 * read as x/z/y halfwords by battle_camera_pick_nearest_unblocked_yaw. */
extern s16 g_battle_scratch_coords[3];
extern s32 g_battle_sort_targets_nearest_first;
extern s32 g_battle_post_battle_unit_changes_active;
extern s16 g_battle_auto_battle_option_colors[5]; /* text colour per option */
extern s32 g_battle_formation_display_mode;
extern u8 g_battle_mimic_pending_flags[21];
extern s16 g_battle_preview_target_unit_id;
extern u16 g_battle_saved_animation_speed; /* animation speed saved while an event thread forces speed 2 */

/* Provisional: the 12-entry zodiac compatibility table indexed by the absolute
 * difference of the two signs.  1/4 weaken the formula, 2/5 strengthen it, and
 * 3 marks the opposition pair whose result depends on sex. */
extern u8 g_battle_zodiac_compatibility_modifiers[];
extern u16 g_battle_team_golem[4]; /* 0x8018f5f4 */
extern void* g_battle_formation_screen_active;

/* BATTLE flag at 0x80165ff2, set by etc_graphic_open while a full-screen ETC
 * graphic is shown; BATTLE then draws primitives immediately instead of
 * linking them into the ordering table. */
extern u16 g_battle_etc_graphics_enabled;
extern u16 g_battle_field_object_use_request; /* 0x80165fe4 */
extern u16 g_battle_field_object_wait_status; /* 0x80166070 */
extern u8 g_geomancy_terrain_ability_table[];
extern job_data_t* g_job_data_pointer;
extern s32 g_max_battle_frame_measurement;
s32 battle_classify_character_identity_slot(u32 character_identity);
s32 battle_return_zero_801842f8(void);
s32 battle_is_skillset_in_spell_quote_exception_list(s32 skillset_id);
void battle_process_inflict_status_commands(void);
void* bzero(void* destination, s32 byte_length);
s32 battle_poll_companion_executable_request(void);
s32 battle_fixed_cross_product_q12(s32 a, s32 b, s32 c, s32 d);
void save_3_u16(SVECTOR* output, u16 x, u16 y, u16 z);
s32 battle_prepare_terrain_poison(battle_stats_t* unit);
s32 battle_return_one(void);
void battle_write_packed_nibble(u8* data, s32 index, s32 row, s32 stride, s32 value);
s32 battle_spread_targeting_panel_to_neighbors(s32 y, s32 x);
s32 battle_return_zero(void);
void blit_text_glyph(void* text, void* pixels, void* glyph, void* position);
void battle_world_display_specific_menu_text(s32 buffer, s32 position, s32 text);

/* unnamed */
extern s16 D_800b628c;
extern s16 D_800b6290;
extern s32 D_800F668C;

/* Write-only: zeroed with the map render state and never read anywhere on
 * the disc. The 150 x 14-byte shape suggests per-instruction parameter
 * storage (an event opcode takes at most 14 parameters), but no code proves
 * it, so the names stay provisional. */
extern u8 D_800F70B4[150][14];
extern u8 D_800F78E8[150];
extern s16 D_801669E8;
extern s32 D_80173C70;
extern u8 D_8018F4FC;
extern u8 D_8018F7CC;
extern u8 D_8018F7D0;
extern s32 D_8018F7D8;
extern u8 D_8019387C;
extern s32* D_801B8A34;
extern s32 D_801B8B18;
extern s32 D_801B8B1C;
extern s32 D_801B8B98;
extern s32 D_801BACC4;
extern s32 D_801BC0C4;
extern s32 D_801BC0D4;
extern s32 D_801BC0D8;

/* Written by battle_effect_code_script_05 but never read. */
extern u16 D_801BF000;

#endif
