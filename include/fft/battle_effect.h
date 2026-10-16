#ifndef BATTLE_EFFECT_H
#define BATTLE_EFFECT_H

/* Resident effect runtime and layouts shared with the ability-effect overlays:
 * scripts, records, particles, timelines and rendering. Native overlay callbacks
 * and their work records are declared in effect.h. */

#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* callbacks */
/* Native effect handlers receive the record, callback slot, emitter and frame.
 * Individual handlers narrow the record index or ignore unused arguments. */
typedef s32 (*effect_callback_t)(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* Resident catalog entries used by effect script opcodes 06 and 07. */
enum { EFFECT_NATIVE_CALLBACK_COUNT = 198 };

typedef enum effect_secondary_phase {
    EFFECT_SECONDARY_PHASE_ENDED = 0, /* the slot is free */
    EFFECT_SECONDARY_PHASE_INITIALIZING = 1,
    EFFECT_SECONDARY_PHASE_EXECUTING = 2,
    EFFECT_SECONDARY_PHASE_FINALIZING = 3
} effect_secondary_phase_e;

/* Sixteen secondary effects may run at once; record zero anchors their chains. */
enum {
    EFFECT_SECONDARY_SLOT_COUNT = 16,
    EFFECT_SECONDARY_RECORD_COUNT = EFFECT_SECONDARY_SLOT_COUNT + 1,
};

/* Verified prefix only: BATTLE 0x801aa7c4 reads/writes a 16-bit value at +6.
 * Neither the full record nor the individual bit meanings are known. */
typedef struct effect_flag_prefix {
    u8 _unused_00[6];
    u16 flags;
} effect_flag_prefix_t;
typedef char effect_flag_prefix_size_must_be_8[(sizeof(effect_flag_prefix_t) == 8) ? 1 : -1];

/* targets */
/* target_type of an effect target record. */
typedef enum effect_target_type {
    EFFECT_TARGET_TYPE_UNIT = 0,
    EFFECT_TARGET_TYPE_TILE = 1,
} effect_target_type_e;

enum {
    EFFECT_TARGET_COUNT_MAX = BATTLE_UNIT_RUNTIME_SLOT_COUNT,
    EFFECT_CASTER_ENTRY = EFFECT_TARGET_COUNT_MAX, /* index of the caster's record in effect_secondary_init_t.targets */
    /* the target index that makes battle_effect_resolve_target_world_coords resolve the caster */
    EFFECT_TARGET_INDEX_CASTER = -1,
};

/* Ten-byte target records at g_battle_effect_context.targets (0x801bad10).
 * Unit targets store a runtime id at 0x02; tile targets reuse that 16-bit value for X
 * and carry map Z/Y at 0x04/0x06. Entry EFFECT_CASTER_ENTRY is the
 * caster. */
typedef struct effect_target {
    u16 target_type; /* 0x00; effect_target_type_e */
    union {
        struct {
            u8 runtime_id;
            u8 result_animation; /* effect_hit_result_e */
        } unit;
        SVECTOR tile; /* x, layer, y */
    } data;           /* 0x02 */
} effect_target_t;
typedef char effect_target_size_must_be_10[(sizeof(effect_target_t) == 10) ? 1 : -1];

/* secondary effects */
/* One running secondary effect: a 0x54-byte record of
 * g_battle_effect_secondary_data (0x801b8b9c), ids 1-16 with record 0 the
 * sentinel. The secondary effects are the ones resident in BATTLE.BIN (weapon
 * hits, projectiles, the charges, the level-up and so on): each is an
 * animation id run by its handler of g_battle_effect_secondary_handlers
 * through the phases of effect_secondary_phase_e. Records chain by id.
 * The particles a handler draws are effect slots (effect_slot_t), a
 * separate pool, whose ids it keeps in slot_ids or in its own state. */
typedef struct effect_secondary_data {
    u8 previous_slot_id; /* 0x00; the record before it on the active chain, or the next free record */
    u8 next_slot_id;     /* 0x01 */
    u8 own_slot_id;      /* 0x02; index of this record in g_battle_effect_secondary_data */
    u8 function_id;      /* 0x03; index of its handler in g_battle_effect_secondary_handlers */
    u16 animation_id;    /* 0x04; effect_secondary_animation_id_e */
    /* 0x06; the animation's own: the charge's element mask, then its effect_element_index_e; the
     * weapon hit's element index; the caster's sprite palette; the frames a reflect shield stays */
    u16 parameter;
    effect_secondary_phase_e phase; /* 0x08 */
    s16 timer;                      /* 0x0c; frames the handler has run */
    u8 _unused_0e[2];
    effect_target_t caster; /* 0x10; copied from the init record's caster */
    union {
        effect_target_t record; /* from the init record's target */
        struct {
            s16 _unused_00;
            s16 x; /* 0x1c; fall-dust position, 1/16 of the real coordinate */
            s16 z; /* 0x1e */
            s16 y; /* 0x20 */
        } position;
    } target;           /* 0x1a */
    s32 used_weapon_id; /* 0x24; init record used_weapon_id */
    s32 target_count;   /* 0x28; init record target_count (projectile setup) */
    /* 0x2c; live effect slots of its particle group: battle_effect_start_group counts them up and
     * battle_effect_update_active_slots down; the handlers finish at 0. */
    s16 active_count;
    s16 finish_timer;  /* 0x2e; counts frames after the last slot finished */
    u8 slot_ids[0x20]; /* 0x30; its effect slots, 0 for none; cleared when the record is allocated */
    void* allocation;  /* 0x50; the handler's work block; freed when the record is removed */
} effect_secondary_data_t;
typedef char effect_secondary_data_size_must_be_84[(sizeof(effect_secondary_data_t) == 84) ? 1 : -1];

/* Complete 0xc8-byte effect context: built on the stack for secondary effects
 * and copied whole to g_battle_effect_context for the ability effect. */
typedef struct effect_secondary_init {
    u16 target_count;   /* 0x00; effect targets: one child record each, spawn_delay apart */
    u8 spectator_count; /* 0x02; entries in spectator_ids */
    u8 math_skill_flag; /* 0x03; enables the effect flags' spawn_delay_override */
    effect_target_t targets[EFFECT_TARGET_COUNT_MAX + 1]; /* 0x04; caster at EFFECT_CASTER_ENTRY */
    /* 0xae; living units that are neither targets nor the caster; also accessed
     * through the separately bound g_battle_effect_palette_target_runtime_ids. */
    u8 spectator_ids[14];
    effect_target_t target_tile; /* 0xbc; post-action destination tile */
    u16 used_weapon_id;          /* 0xc6 */
} effect_secondary_init_t;
typedef char effect_secondary_init_size_must_be_200[(sizeof(effect_secondary_init_t) == 200) ? 1 : -1];

/* result_animation of a target in an effect init record. The records built by
 * battle_effect_store_targets_and_display_types use the first four; the weapon-hit
 * records (battle_effect_build_secondary_init_from_action) use all but REFLECTED. */
typedef enum effect_hit_result {
    EFFECT_HIT_NORMAL = 0,
    EFFECT_HIT_CRITICAL = 1,
    EFFECT_HIT_REFLECTED = 2,
    EFFECT_HIT_MISSED = 3,    /* weapon hits: evaded by an accessory, a hand or the class */
    EFFECT_HIT_NO_EFFECT = 4, /* nullified, missed outright, canceled, grasped or caught */
    EFFECT_HIT_GOLEM = 5,
    EFFECT_HIT_BREAK = 6,     /* equipment broken */
    EFFECT_HIT_BREAK_ALT = 7, /* handled like BREAK; nothing stores it */
} effect_hit_result_e;

/* The secondary animation parameter that makes the projectile handler draw the thrown item's own
 * graphic instead of a model. */
enum { EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC = -1 };

/* Four-byte records at 0x801b84dc indexed by animation id
 * (effect_secondary_animation_id_e); byte 0 selects the
 * g_battle_effect_secondary_handlers handler. */
typedef struct effect_secondary_animation {
    u8 function_id; /* 0x00 */
    u8 _padding_01; /* 0x01; aligns parameter */
    /* 0x02; the projectile handlers: effect_model_e in the low byte, or
     * EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC; the charge handler: its parameter row */
    s16 parameter;
} effect_secondary_animation_t;

/* Queue of post-action "effect message" popups, drained newest-first by
 * action_resume_attack_phase_control. The stride is 8: the producer at
 * 0x80072d8c indexes the array directly and the consumer walks it by
 * `idx * 8`, and only these three fields are ever read or written. */
typedef struct battle_post_effect_msg {
    u8 code;           /* 0x00 message code passed to menu_init_system_function */
    u8 unit;           /* 0x01 runtime ID of the acting unit */
    u8 _padding_02[2]; /* 0x02; aligns value */
    s32 value;         /* 0x04 reported amount */
} battle_post_effect_msg_t;

/* The projectile models of g_battle_effect_model_files. */
typedef enum effect_model {
    EFFECT_MODEL_ARROW = 0,
    EFFECT_MODEL_STONE = 1,
    EFFECT_MODEL_REFLECT = 2,
    EFFECT_MODEL_SHURIKEN = 3, /* drawn for the thrown items of ITEM_TYPE_SHURIKEN */
} effect_model_e;

/* The inertia threshold every secondary effect handler runs its slots under, with a gravity of
 * (0, ONE, 0); an effect file's script sets its own pair (opcode 27). */
enum { EFFECT_SECONDARY_INERTIA_THRESHOLD = 560 };

/* secondary state */
/* One 14-byte parameter row for the spell-charge secondary effect. */
typedef struct effect_charge_params {
    s16 speed;
    s16 spread_range;
    s16 spread_offset;
    s16 duration;
    s16 radius;
    u8 spawn_interval;
    u8 _unused_0b;
    u8 max_trails;
    u8 fade_row;
} effect_charge_params_t;
typedef char effect_charge_params_size_must_be_14[sizeof(effect_charge_params_t) == 14 ? 1 : -1];

/* Per-secondary-slot spawn state for the two-particle death poof. */
typedef struct effect_death_poof_state {
    s32 count;        /* 0x00; slots started so far */
    u16 finish_timer; /* 0x04; reaches 1 when the effect is finished */
    u8 ids[2];        /* 0x06 */
} effect_death_poof_state_t;
typedef char effect_death_poof_state_size_must_be_8[sizeof(effect_death_poof_state_t) == 8 ? 1 : -1];

typedef s32 (*effect_secondary_callback_t)(void);

/* One trail of the spell charge effect: the recent screen positions of the effect slot it follows. */
typedef struct effect_charge_trail {
    DVECTOR points[7]; /* 0x00; screen positions, newest at age % 7 */
    s16 age;           /* 0x1c */
    u8 slot_id;        /* 0x1e */
    u8 _padding_1f;    /* tail padding to 2-byte alignment */
} effect_charge_trail_t;

typedef struct effect_charge_state {
    s16 x;                            /* 0x0000 */
    s16 y;                            /* 0x0002 */
    DR_MODE mode[2];                  /* 0x0004 */
    LINE_G2 lines[2][96];             /* 0x001c */
    effect_charge_trail_t trails[16]; /* 0x0f1c */
    u8 ids_a[2];                      /* 0x111c */
    u8 ids_b[14];                     /* 0x111e */
    struct {
        s16 trails;
        u8 auxiliary_particles;
        u8 particles;
    } count;            /* 0x112c */
    s32 angle;          /* 0x1130 */
    s32 speed;          /* 0x1134 */
    s32 spread_range;   /* 0x1138 */
    s32 spread_offset;  /* 0x113c */
    s32 duration;       /* 0x1140 */
    s32 radius;         /* 0x1144 */
    s32 spawn_interval; /* 0x1148 */
    s32 color;          /* 0x114c */
    s32 max_trails;     /* 0x1150 */
    s32 fade_row;       /* 0x1154 */
    s32 done;           /* 0x1158 */
} effect_charge_state_t;

/* Summon-charge state: three rings of ten trailing points. */
typedef struct effect_ring_state {
    s32 index;             /* 0x00; newest trail column, 0..9 */
    s32 radius;            /* 0x04 */
    s32 angle;             /* 0x08 */
    s32 angle_step;        /* 0x0c */
    SVECTOR center;        /* 0x10 */
    s32 brightness;        /* 0x18 */
    SVECTOR points[3][10]; /* 0x1c */
} effect_ring_state_t;

/* Zodiac-poof spawn state for the 24-slot particle group 3. */
typedef struct effect_trap_state_24 {
    s32 done;   /* 0x00; reaches 1 when the effect is finished */
    s32 count;  /* 0x04; slots started so far */
    u8 ids[24]; /* 0x08 */
} effect_trap_state_24_t;

/* Work area at 0x801bade0 shared by the secondary effect handlers; the
 * spell-charge, summon-charge and zodiac-poof handlers each lay it out as
 * their own state. */
typedef union effect_secondary_state {
    effect_charge_state_t charge;
    effect_ring_state_t summon_ring;
    effect_trap_state_24_t zodiac_poof;
} effect_secondary_state_t;

/* timelines */
/* One 0x80-byte particle channel of an effect timeline. Keyframe k lasts
 * from frame_start[k] to frame_start[k + 1]. Entering it runs its 16-bit action value
 * (unit reactions, and binding a native handler to a callback slot); on each
 * of its frames it spawns its emitter, through the handler of the action's
 * callback slot when that is set. Keyframe 0 is passed over without spawning;
 * emitter[k - 1] belongs to keyframe k. Retail channels have up to 24 keyframes. */
typedef struct effect_keyframe_table {
    u16 frame_start[25]; /* 0x00: keyframe boundaries */
    u8 emitter[24];      /* 0x32: keyframe 1 starts at element 0; emitter index + 1, 0 for none */
    u16 action[25];      /* 0x4a: EFFECT_KEYFRAME_ACTION_*, including the final transition */
    u8 _unused_7c[2];    /* 0x7c */
    s16 last_index;      /* 0x7e; highest valid keyframe index */
} effect_keyframe_table_t;
typedef char effect_keyframe_table_size_must_be_128[(sizeof(effect_keyframe_table_t) == 128) ? 1 : -1];

enum {
    EFFECT_PALETTE_TRACK_KEYFRAMES = 33,
};

/* The screen color track: the palette-track layout with a second color per
 * keyframe. An APPLY keyframe shifts the map background gradient by twice
 * its color; any other sets the gradient's two stops to its two colors. */
typedef struct effect_background_track {
    s16 duration[EFFECT_PALETTE_TRACK_KEYFRAMES];
    rgb_s8_t color[EFFECT_PALETTE_TRACK_KEYFRAMES];
    rgb_s8_t second_color[EFFECT_PALETTE_TRACK_KEYFRAMES];
    u8 preset[EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} effect_background_track_t;
typedef char effect_background_track_size_must_be_300[(sizeof(effect_background_track_t) == 300) ? 1 : -1];

/* preset of a color track keyframe. */
enum {
    EFFECT_COLOR_KEYFRAME_MODE_MASK = 0x7f, /* the map color mode (MAP_COLOR_MODE_*) the color is applied in */
    EFFECT_COLOR_KEYFRAME_APPLY = 0x80,     /* clear on the unit tracks: the keyframe only waits */
};

/* A color track of an effect timeline: timed palette changes of the
 * surroundings, the caster or the targets. A keyframe lasts duration * 8
 * frames (one frame for 0); a track of count keyframes plays count - 1. */
typedef struct effect_palette_track {
    s16 duration[EFFECT_PALETTE_TRACK_KEYFRAMES];
    rgb_s8_t color[EFFECT_PALETTE_TRACK_KEYFRAMES];
    u8 preset[EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} effect_palette_track_t;
typedef char effect_palette_track_size_must_be_200[sizeof(effect_palette_track_t) == 200 ? 1 : -1];

/* One phase's color tracks. The surroundings are the map's field color and
 * the spectators: the living units that are neither a target nor the caster. */
typedef struct effect_timeline_color_tracks {
    effect_palette_track_t surroundings; /* 0x000: the map and the units that only look on */
    effect_palette_track_t caster;       /* 0x0c8 */
    effect_palette_track_t target;       /* 0x190 */
    effect_background_track_t screen;    /* 0x258 */
} effect_timeline_color_tracks_t;

enum {
    EFFECT_PARTICLE_CHANNEL_COUNT = 5,
    EFFECT_SOUND_TRACK_COUNT = 3,
};

/* A sound track of the child timeline: up to 17 entries, each naming an
 * effect sound channel to play when it is entered. The parent timeline's
 * tracks hold nine (effect_timeline_t.sound_tracks). */
typedef struct effect_child_sound_track {
    u16 duration[17]; /* 0x00; frames each entry lasts */
    u8 channel[17];   /* 0x22; 0 and 1 none, otherwise a sound channel of the flags section + 2 */
    u8 _padding_33;   /* aligns count */
    s16 count;        /* 0x34 */
} effect_child_sound_track_t;

/* The child timeline, eight bytes into the timeline section
 * (g_battle_effect_child_timeline): its duration and its channels.
 * effect_timeline_t names the section's other parts. */
typedef struct effect_child_timeline {
    u16 _unused_000;
    u16 duration; /* 0x002; frames: effect_timeline_t.child.duration */
    effect_keyframe_table_t particle[EFFECT_PARTICLE_CHANNEL_COUNT]; /* 0x004 */
    effect_child_sound_track_t sound[EFFECT_SOUND_TRACK_COUNT];      /* 0x284 */
    effect_palette_track_t surroundings;                             /* 0x326 */
    effect_palette_track_t caster;                                   /* 0x3ee */
    effect_palette_track_t target;                                   /* 0x4b6 */
    effect_background_track_t screen;                                /* 0x57e */
} effect_child_timeline_t;
typedef char effect_child_timeline_size_must_be_1706[sizeof(effect_child_timeline_t) == 1706 ? 1 : -1];

/* target_mode of battle_effect_start_new. */
typedef enum effect_start_target {
    EFFECT_START_ON_UNIT = 0,
    EFFECT_START_ON_TILE = 1,
    EFFECT_START_ON_CASTER = 2, /* a unit that is both caster and target */
} effect_start_target_e;

/* loading and playback */
/* target_mode selects the unit id or the tile coordinate pointer. */
typedef union effect_start_target_data {
    s32 runtime_id;
    const SVECTOR* tile;
} effect_start_target_data_t;
typedef char effect_start_target_data_size_must_be_4[sizeof(effect_start_target_data_t) == 4 ? 1 : -1];

/* An ability-to-file entry: file ID bits and the Item flag, negative for none.
 * Event effects follow the action/reaction rows and use the script ID remapping. */
enum {
    EFFECT_ABILITY_FILE_MASK = 0x1ff, /* the number in the file's name, EFFECT/E###.BIN */
    EFFECT_ABILITY_FLAG_ITEM = 0x800, /* Item abilities and Throw Ball: battle_effect_is_item_ability */
};

/* One resident header and disc-table slot for each 9-bit effect file ID. */
enum { EFFECT_FILE_ID_COUNT = 512 };

/* The effect files the code names by number. */
enum {
    EFFECT_FILE_CURE = 1, /* loaded in place of a file of no size */
    EFFECT_FILE_FIRE = 16,
    EFFECT_FILE_BOLT = 20,
    EFFECT_FILE_ICE = 24,
};

/* What battle_effect_get_element_index makes of an element mask: the position of its first element in
 * g_battle_effect_element_masks, 0 for no element. It picks the CLUT of the weapon hit and
 * spell charge particles and the effect of a thrown ball. */
typedef enum effect_element_index {
    EFFECT_ELEMENT_INDEX_NONE = 0,
    EFFECT_ELEMENT_INDEX_FIRE = 1,
    EFFECT_ELEMENT_INDEX_WATER = 2,
    EFFECT_ELEMENT_INDEX_EARTH = 3,
    EFFECT_ELEMENT_INDEX_WIND = 4,
    EFFECT_ELEMENT_INDEX_HOLY = 5,
    EFFECT_ELEMENT_INDEX_DARK = 6,
    EFFECT_ELEMENT_INDEX_ICE = 7,
    EFFECT_ELEMENT_INDEX_LIGHTNING = 8,
} effect_element_index_e;

/* g_battle_effect_load_state: the steps of battle_effect_update_file_load, which loads the ability's effect file
 * while its charge secondary effect plays. */
typedef enum effect_load_state {
    EFFECT_LOAD_START = 0,            /* start reading the file */
    EFFECT_LOAD_READING = 1,          /* wait for the read */
    EFFECT_LOAD_COUNTDOWN = 2,        /* wait out the 60 frames given to a charge secondary effect */
    EFFECT_LOAD_ENDING_SECONDARY = 3, /* wait for the charge secondary effect to end */
} effect_load_state_e;

/* g_battle_effect_playback_phase: the stages of battle_effect_update_stage. */
typedef enum effect_playback_phase {
    EFFECT_PLAYBACK_PHASE_IDLE = 0,
    EFFECT_PLAYBACK_PHASE_REQUESTED = 1, /* waiting for battle_effect_play */
    EFFECT_PLAYBACK_PHASE_INIT_PALETTE = 2,
    EFFECT_PLAYBACK_PHASE_START_SCRIPT = 3, /* bind the effect file's sections and start its script */
    EFFECT_PLAYBACK_PHASE_RUNNING = 4,
    EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT = 6, /* start the script again without rebinding the file */
} effect_playback_phase_e;

/* Opcodes (top header byte) of the effect model commands that battle_effect_decode_next_model_command
 * decodes: each draws the primitive it is named for. Nothing in the code tells the two GT3 opcodes
 * apart except that battle_effect_draw_projectile_model draws only the second. */
enum {
    EFFECT_MODEL_COMMAND_G3 = 0x31,
    EFFECT_MODEL_COMMAND_GT3_35 = 0x35,
    EFFECT_MODEL_COMMAND_GT3_37 = 0x37,
    EFFECT_MODEL_COMMAND_G4 = 0x39,
};

/* Index into g_battle_effect_trap_particle_configs: the particle groups of the secondary effect
 * handlers, named after the handler that spawns each. */
typedef enum effect_particle_group {
    /* battle_effect_weapon_hit_secondary_handler: its first slot set */
    EFFECT_PARTICLE_GROUP_WEAPON_HIT_A = 0,
    EFFECT_PARTICLE_GROUP_WEAPON_HIT_B = 1, /* its second slot set */
    EFFECT_PARTICLE_GROUP_ZODIAC_POOF = 3,
    EFFECT_PARTICLE_GROUP_DEATH_POOF = 4,
    EFFECT_PARTICLE_GROUP_LEVEL_UP = 5,
    EFFECT_PARTICLE_GROUP_FALL_DUST = 6,
    EFFECT_PARTICLE_GROUP_SPLASH = 7,
    EFFECT_PARTICLE_GROUP_VENOM_TRAP = 8,
    EFFECT_PARTICLE_GROUP_WEAPON_HIT_B_MISSED = 9, /* the second slot set of a missed hit */
    EFFECT_PARTICLE_GROUP_RED_ORBS = 10,
    EFFECT_PARTICLE_GROUP_EQUIP_BREAK = 11,
    EFFECT_PARTICLE_GROUP_SPELL_CHARGE = 12,
    EFFECT_PARTICLE_GROUP_SPELL_CHARGE_13 = 13, /* given the height of group 12; nothing spawns it */
    EFFECT_PARTICLE_GROUP_SUMMON_CHARGE = 14,
    EFFECT_PARTICLE_GROUP_TELEPORT = 15,
    EFFECT_PARTICLE_GROUP_GOLEM = 16,
} effect_particle_group_e;

/* Index into g_battle_effect_secondary_animations: the secondary effects resident in BATTLE.BIN. Those
 * the code does not start by number come from two tables: the weapon type's projectile
 * (g_battle_effect_weapon_projectile_animations) and the ability's charge animation set
 * (g_battle_effect_secondary_by_charge_animation). */
typedef enum effect_secondary_animation_id {
    EFFECT_SECONDARY_ANIMATION_NONE = 0,
    EFFECT_SECONDARY_ANIMATION_BOW_ARROW = 1,       /* bows: the arrow model on an arc */
    EFFECT_SECONDARY_ANIMATION_SPELL_CHARGE = 2,    /* charge animation set 1 */
    EFFECT_SECONDARY_ANIMATION_CROSSBOW_BOLT = 3,   /* crossbows: the arrow model on a straight line */
    EFFECT_SECONDARY_ANIMATION_SPELL_CHARGE_04 = 4, /* the charge with parameter row 1; no table names it */
    EFFECT_SECONDARY_ANIMATION_SUMMON_CHARGE = 5,   /* charge animation set 2 */
    EFFECT_SECONDARY_ANIMATION_THROWN_STONE = 6,
    EFFECT_SECONDARY_ANIMATION_WEAPON_HIT = 7,
    EFFECT_SECONDARY_ANIMATION_WEAPON_HIT_08 = 8, /* the weapon hit handler again; nothing starts it */
    EFFECT_SECONDARY_ANIMATION_FALL_DUST = 9,
    EFFECT_SECONDARY_ANIMATION_REFLECT = 10,
    EFFECT_SECONDARY_ANIMATION_ZODIAC_POOF = 11,
    EFFECT_SECONDARY_ANIMATION_DEATH_SMOKE = 12,
    EFFECT_SECONDARY_ANIMATION_LEVEL_UP = 13,
    EFFECT_SECONDARY_ANIMATION_VENOM_TRAP = 14,
    EFFECT_SECONDARY_ANIMATION_SPLASH = 15,
    EFFECT_SECONDARY_ANIMATION_THROWN_ITEM = 16, /* Item and Throw: the item's graphic or the shuriken model */
    EFFECT_SECONDARY_ANIMATION_RED_ORBS = 17,    /* the Charge abilities and Accumulate */
    EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK = 18, /* abilities 0x8a-0x91 */
    EFFECT_SECONDARY_ANIMATION_TELEPORT = 19,
    EFFECT_SECONDARY_ANIMATION_GOLEM = 20,
    /* the null handler; g_battle_effect_ending_secondary_function_id is this record's function byte */
    EFFECT_SECONDARY_ANIMATION_ENDING = 21,
} effect_secondary_animation_id_e;

/* One particle group of the secondary effects: a 0x2e-byte record of
 * g_battle_effect_trap_particle_configs. Its leading bytes schedule the
 * group's effect slots (battle_effect_start_group); the rest is a fixed
 * emitter, from which battle_effect_spawn_particle_motion draws each slot's
 * motion as the effect file emitter does a particle's. */
/* particle motion */
typedef struct effect_emitter_values {
    u8 trap_animation;      /* 0x00; trap groups: index into g_battle_effect_trap_animation_data */
    u8 _unused_01;          /* 0x01 */
    u8 spawn_start_frame;   /* 0x02; trap groups */
    u8 spawn_end_frame;     /* 0x03; trap groups */
    u8 count;               /* 0x04; trap groups: slots the group owns */
    u8 _unused_05;          /* 0x05 */
    u16 anchor_flags;       /* 0x06; effect_emitter_origin_e: CASTER and TARGET are honored */
    u16 motion_flags;       /* 0x08; effect_emitter_velocity_e */
    vec3s16_t position;     /* 0x0a */
    vec3s16_t spread;       /* 0x10 */
    vec3s16_t angle;        /* 0x16 */
    vec3s16_t angle_spread; /* 0x1c */
    ranges16_t weight;      /* 0x22 */
    u16 speed_min;          /* 0x26 */
    u16 speed_max;          /* 0x28 */
    u8 spawns_per_frame;    /* 0x2a; trap groups: slots one frame may start */
    u8 _unused_2b;          /* 0x2b */
    s8 lifetime_min;        /* 0x2c */
    s8 lifetime_max;        /* 0x2d */
} effect_emitter_values_t;
typedef char effect_emitter_values_size_must_be_46[(sizeof(effect_emitter_values_t) == 46) ? 1 : -1];

/* flags of a sprite part (effect_sprite_part_t, effect_sprite_frameset_t). */
enum {
    EFFECT_SPRITE_PART_CLUT_MASK = 0x000f,
    EFFECT_SPRITE_PART_FLAG_SECOND_CLUT_ROW = 0x0010,  /* the CLUT is of MAP_CLUT_ROW_EFFECT_SECOND */
    EFFECT_SPRITE_PART_TPAGE_MODE_MASK = 0x00e0,       /* the tpage's blend rate and 8-bit color bits */
    EFFECT_SPRITE_PART_FLAG_SEMI_TRANSPARENT = 0x0200, /* draw it semi-transparent */
};

/* One textured quad of a multi-part effect sprite: a frame of an effect file's
 * frame data holds a count of them. */
typedef struct effect_sprite_part {
    u16 flags; /* 0x00; EFFECT_SPRITE_PART_*; the tpage mode bits repeat those of tpage and are not read here */
    u16 tpage; /* 0x02 */
    u8 u;
    u8 v;
    u8 width;
    u8 height;
    /* Four-byte alignment preserves the original x/y 32-bit copies. */
    DVECTOR corners[4] __attribute__((aligned(4))); /* 0x08 */
} effect_sprite_part_t;
typedef char effect_sprite_part_size_must_be_24[(sizeof(effect_sprite_part_t) == 24) ? 1 : -1];

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

/* projectile geometry */
/* Query read by battle_effect_calculate_arcing_trajectory: the offset to the
 * target and both endpoints. */
typedef struct battle_arc_query {
    battle_arc_delta_t delta;
    battle_arc_endpoint_t source;
    battle_arc_endpoint_t target;
} battle_arc_query_t;

/* Unit list built in the scratchpad at 0x1f800000 by
 * battle_effect_list_valid_unit_targeting_data (0x801af770) and read by the
 * obstacle tests. */
typedef struct effect_obstacle_unit_list {
    s32 count;                                  /* 0x00 */
    u8 battle_ids[BATTLE_UNIT_SLOT_COUNT];      /* 0x04 */
    u8 _padding_19;                             /* 0x19; aligns the position vectors */
    SVECTOR positions[BATTLE_UNIT_SLOT_COUNT];  /* 0x1a; tile center and height */
    u8 _padding_c2[2];                          /* 0xc2; aligns the sprite heights */
    s32 sprite_heights[BATTLE_UNIT_SLOT_COUNT]; /* 0xc4 */
} effect_obstacle_unit_list_t;
typedef char assert_effect_obstacle_unit_list_size[sizeof(effect_obstacle_unit_list_t) == 280 ? 1 : -1];

/* Shared output of the model-command decoder and input of the projectile renderer. */
typedef struct effect_decoded_model_command {
    struct {
        u8 _unused_00;
        u8 word_count;
        u8 _unused_02;
        u8 opcode;
    } header;          /* 0x00 */
    CVECTOR colors[4]; /* 0x04 */
    struct {
        u16 tpage;
        u16 clut;
        uv8_t uv[3];
    } texture; /* 0x14 */
    u8 _unused_1e[0x4a];
    /* The decoder copies two packed pairs; the renderer reads four indices. */
    union {
        u32 pairs[2];
        u16 indices[4];
    } vertices; /* 0x68 */
} effect_decoded_model_command_t;
typedef char effect_decoded_model_command_size_must_be_112[sizeof(effect_decoded_model_command_t) == 112 ? 1 : -1];

/* 0xc88-byte arrow work buffer of the bow-arrow (0x801b0ffc) and projectile
 * (0x801b3938) secondary handlers: two sets of four G3 and thirteen G4
 * polygons. */
typedef struct effect_arrow_buffer {
    POLY_G3 g3[2][4];  /* 0x000 */
    POLY_G4 g4[2][13]; /* 0x0e0 */
    u8 _unused_488[0x800];
} effect_arrow_buffer_t;
typedef char effect_arrow_buffer_size_must_be_3208[(sizeof(effect_arrow_buffer_t) == 3208) ? 1 : -1];

/* Double-buffered polygons initialized by the secondary handler and drawn by
 * battle_effect_draw_projectile_model. */
typedef struct effect_stone_buffer {
    POLY_G3 g3[2][12];
} effect_stone_buffer_t;

typedef struct effect_shuriken_buffer {
    POLY_G3 g3[2][16];
} effect_shuriken_buffer_t;

typedef struct effect_reflect_buffer {
    u16 clut[2][6];
    POLY_GT3 prims[2][24];
} effect_reflect_buffer_t;

/* 21-keyframe camera table of the parent timeline's two phases. A keyframe
 * moves the tracks its 16-bit command names (EFFECT_CAMERA_TRACK_*)
 * toward its values, arriving at end_frame; a track takes its next keyframe
 * only once its previous move has ended. */
typedef struct effect_camera_keyframes_21 {
    s16 end_frame[21];      /* frame the keyframe's moves arrive, counted from the start of the table's phase */
    vec3s16_t rotation[21]; /* pitch, yaw, roll */
    vec3s16_t position[21];
    vec3s16_t zoom[21];
    s16 command[21]; /* EFFECT_CAMERA_TRACK_* | effect_camera_source_e | effect_camera_mode_e */
    s16 last_keyframe_index;
} effect_camera_keyframes_21_t;

/* A parent-timeline sound track: channel 0/1 is silent; other values select
 * the effect's sound channel plus two. */
typedef struct effect_sound_track {
    u16 durations[9]; /* 0x00 */
    u8 channels[9];   /* 0x12 */
    u8 _padding_1b;   /* aligns entry_count */
    s16 entry_count;  /* 0x1c */
} effect_sound_track_t;

/* The timeline section of an effect file (effect_file_header_t.timeline).
 *
 * A script that spawns children (opcode 29) runs the parent timeline in three
 * stages: phase 1 for phase1_duration frames; then one child record per
 * effect target, spawn_delay frames apart, each running the child timeline
 * for child.duration frames; then phase 2, which starts when the last child's
 * timeline ends, for phase2_duration frames. A script without children
 * (opcode 28 on the root record) runs only the child timeline, once.
 *
 * The child timeline's channels are the effect_child_timeline_t that
 * g_battle_effect_child_timeline addresses at 0x008; its camera table is the
 * 17-keyframe one at 0x6b2, laid out like effect_camera_keyframes_21_t. Each
 * phase has five particle channels, three sound tracks, four color tracks
 * and a 21-keyframe camera table. */
typedef struct effect_timeline {
    u16 _unused_0000;
    u16 phase2_duration;           /* 0x002 */
    u16 phase1_duration;           /* 0x004 */
    u16 spawn_delay;               /* 0x006; frames between child spawns (see effect_flags_section_t) */
    effect_child_timeline_t child; /* 0x008; child duration, particle, sound and color tracks */
    s16 end_frame[17];             /* 0x6b2; child camera table: frame each keyframe's moves arrive */
    vec3s16_t rotation[17];        /* 0x6d4 */
    vec3s16_t position[17];        /* 0x73a */
    vec3s16_t zoom[17];            /* 0x7a0 */
    s16 command[17];               /* 0x806; as effect_camera_keyframes_21_t.command */
    s16 last_keyframe_index;       /* 0x828; highest valid keyframe index */
    effect_keyframe_table_t particle_channels[2][EFFECT_PARTICLE_CHANNEL_COUNT]; /* 0x82a; [phase][channel] */
    effect_sound_track_t sound_tracks[2][EFFECT_SOUND_TRACK_COUNT];              /* 0xd2a; [phase][track] */
    effect_timeline_color_tracks_t color_tracks[2];                              /* 0xdde */
    effect_camera_keyframes_21_t phase1_camera;                                  /* 0x14e6 */
    effect_camera_keyframes_21_t phase2_camera;                                  /* 0x16b6 */
} effect_timeline_t;

typedef char effect_timeline_size_must_be_6278[(sizeof(effect_timeline_t) == 6278) ? 1 : -1];

/* Byte offset to a group's frame-offset table, relative to the group-table base. */
typedef struct effect_sprite_group {
    s16 frame_table_offset;
} effect_sprite_group_t;

/* Frame sections start with a group count; group offsets are relative to this table. */
typedef struct effect_sprite_section {
    u8 group_count;                  /* 0x00 */
    u8 _unused_01[3];                /* 0x01; reserved header bytes */
    effect_sprite_group_t groups[0]; /* 0x04; variable-length group table */
} effect_sprite_section_t;
typedef char effect_sprite_section_size_must_be_4[sizeof(effect_sprite_section_t) == 4 ? 1 : -1];

/* One frame selected by the animation stream; its parts render together. */
typedef struct effect_sprite_frameset {
    u16 flags;                     /* 0x00; EFFECT_SPRITE_PART_*: SEMI_TRANSPARENT and the tpage mode bits */
    s16 count;                     /* 0x02; number of sprite parts */
    effect_sprite_part_t parts[0]; /* 0x04; variable-length part table */
} effect_sprite_frameset_t;
typedef char effect_sprite_frameset_size_must_be_4[sizeof(effect_sprite_frameset_t) == 4 ? 1 : -1];

/* Section table at the start of an effect file's header section: byte offsets
 * from the table itself, each read by its section's binder in effect_script.c
 * (battle_effect_set_*_pointer and the flags, sound and palette loaders). */
typedef struct effect_file_header {
    s32 frame_data;          /* 0x00: the binder skips the section's leading 32-bit value */
    s32 animation_sequences; /* 0x04: likewise */
    s32 script;              /* 0x08: effect script section */
    s32 particle_system;     /* 0x0c: emitter motion and control */
    s32 animation_curve;     /* 0x10 */
    s32 timing_curves;       /* 0x14 */
    s32 flags_section;       /* 0x18 */
    s32 timeline;            /* 0x1c: timing, palette and camera tracks */
    s32 sound_section;       /* 0x20: 0 when the effect has no sound */
    s32 palette_and_texture; /* 0x24: two CLUT blocks followed by the image */
} effect_file_header_t;
typedef char effect_file_header_size_must_be_40[sizeof(effect_file_header_t) == 40 ? 1 : -1];

/* Two palette rows, then a 24-bit image size, width flag and pixel tail. */
typedef struct effect_palette_section {
    clut_t primary_clut;   /* 0x000 */
    clut_t secondary_clut; /* 0x200 */
    u8 image_size[3];      /* 0x400; little-endian bytes */
    u8 wide_image;         /* 0x403; 256-byte rows when nonzero, otherwise 128 */
    u32 pixels[0];         /* 0x404; packed 32-bit GPU image-transfer values */
} effect_palette_section_t;
typedef char effect_palette_section_size_must_be_1028[sizeof(effect_palette_section_t) == 1028 ? 1 : -1];

/* script */
/* An effect script instruction starts with a 16-bit header, its opcode in the low nine bits
 * (g_battle_effect_instructions holds 120 handlers; retail scripts stay below
 * 2e) and small operands in the bits above, followed by up to three operand
 * 16-bit values. Script offsets are bytes from the start of the script. */
enum { EFFECT_SCRIPT_OPCODE_MASK = 0x1ff };

/* Each opcode interprets the selector byte differently. Unnamed bits are ignored
 * by that view. Copy the byte before reading bit fields to retain byte loads. */
typedef union effect_script_parameters {
    struct {
        u8 : 4;
        u8 high_nibble : 4;
    } opcode_05;
    struct {
        u8 : 4;
        u8 slot : 4;
    } child;
    struct {
        u8 : 4;
        u8 slot : 2;
        u8 : 2;
    } free_child;
    struct {
        u8 : 6;
        u8 slot : 2;
    } counter;
    struct {
        u8 : 2;
        u8 slot : 6;
    } callback;
    struct {
        u8 : 2;
        u8 count : 6;
    } targets;
} effect_script_parameters_t;
typedef char effect_script_parameters_size_must_be_1[sizeof(effect_script_parameters_t) == 1 ? 1 : -1];

/* 16-bit-aligned instructions have a nine-bit opcode and seven selector bits. */
typedef struct effect_script_header {
    u8 opcode_low;                         /* 0x00 */
    effect_script_parameters_t parameters; /* 0x01: opcode bit 8 and opcode-specific selectors */
} effect_script_header_t;
typedef char effect_script_header_size_must_be_2[(sizeof(effect_script_header_t) == 2) ? 1 : -1];

/* A view at one instruction boundary in a variable-length byte stream.
 * Immediate, branch and camera instructions occupy 4, 6 and 8 bytes respectively;
 * two-byte instructions contain only the header. No array uses this union's stride. */
typedef union effect_script_instruction {
    effect_script_header_t header;
    struct {
        effect_script_header_t header; /* 0x00 */
        s16 value;                     /* 0x02 */
    } immediate;
    struct {
        effect_script_header_t header; /* 0x00 */
        s16 comparison;                /* 0x02 */
        u16 target_offset;             /* 0x04: script byte offset */
    } branch;
    struct {
        effect_script_header_t header; /* 0x00 */
        s16 x;                         /* 0x02 */
        s16 y;                         /* 0x04 */
        s16 z;                         /* 0x06 */
    } vector;
} effect_script_instruction_t;
typedef char effect_script_instruction_size_must_be_8[sizeof(effect_script_instruction_t) == 8 ? 1 : -1];

/* What an opcode handler returns to battle_effect_run_block. */
typedef enum effect_script_result {
    EFFECT_SCRIPT_RESULT_YIELD = 0,    /* stop for this frame */
    EFFECT_SCRIPT_RESULT_CONTINUE = 1, /* run the next instruction */
    EFFECT_SCRIPT_RESULT_ABORT = 2,    /* the script has ended: free the record */
} effect_script_result_e;

/* The particle updater reserves -1 for animation-driven lifetime. */
enum { EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN = -1 };

/* How an effect sound channel picks among its three sound ids on each use. */
typedef enum effect_sound_mode {
    EFFECT_SOUND_MODE_FIRST = 0,                /* always the first */
    EFFECT_SOUND_MODE_ALTERNATE = 1,            /* the first and second in turn */
    EFFECT_SOUND_MODE_FIRST_THEN_SECOND = 2,    /* the first once, then the second */
    EFFECT_SOUND_MODE_FIRST_THEN_ALTERNATE = 3, /* the first once, then the third and second in turn */
    EFFECT_SOUND_MODE_CYCLE = 4,                /* the three in turn */
} effect_sound_mode_e;

/* One of the four sound channels of an effect: the sound tracks of the
 * timelines name a channel, and each use plays the id its mode picks. */
typedef struct effect_sound_channel {
    u8 mode;         /* 0x00; effect_sound_mode_e */
    u8 sound_ids[3]; /* 0x01; ids in the effect's sound section, 0 for none */
} effect_sound_channel_t;

/* Effect flags section at header[0x18] of an effect file. The engine loads
 * flags as a 32-bit value and tests single bits; spawn_delay_override replaces the
 * timeline's spawn_delay only while g_battle_effect_context.math_skill_flag is
 * set. */
typedef struct effect_flags_section {
    u32 flags;                          /* 0x00; EFFECT_FLAG_* bits */
    u8 spawn_delay_override;            /* 0x04 */
    u8 _unused_05[3];                   /* 0x05 */
    effect_sound_channel_t channels[4]; /* 0x08 */
} effect_flags_section_t;
typedef char effect_flags_section_size_must_be_24[(sizeof(effect_flags_section_t) == 24) ? 1 : -1];

/* effect_flags_section_t::flags bits read by the engine. */
enum {
    /* caster and target anchors take the tile's surface height: battle_effect_resolve_target_world_coords */
    EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST = 0x08,
    EFFECT_FLAG_QUIET_MUSIC = 0x10,         /* lower the music while the effect loads and plays */
    EFFECT_FLAG_TIMING_CURVE_PHASE1 = 0x20, /* phase 1 of the parent timeline sets the time scale from its curve */
    EFFECT_FLAG_TIMING_CURVE_CHILD = 0x40,  /* a child timeline sets the time scale from its curve */
};

/* Per-handler phase byte shared by every EFFECT overlay state machine. */
typedef enum effect_callback_phase {
    EFFECT_CALLBACK_PHASE_IDLE = 0,
    EFFECT_CALLBACK_PHASE_INIT = 1,
    EFFECT_CALLBACK_PHASE_UPDATE = 2,
    EFFECT_CALLBACK_PHASE_DESTROY = 3,
} effect_callback_phase_e;

/* Emitter spawn origin: bits 9-11 of an emitter's 16-bit placement flags (byte 0x03
 * mask 0x0e). 0xa00 is the map center:
 * every handler adds half the map geometry width and depth there. */
typedef enum effect_emitter_origin {
    EFFECT_EMITTER_ORIGIN_WORLD = 0x000,
    EFFECT_EMITTER_ORIGIN_CURSOR_TILE = 0x200, /* center of g_battle_effect_target_tile */
    EFFECT_EMITTER_ORIGIN_CASTER = 0x400,
    EFFECT_EMITTER_ORIGIN_TARGET = 0x600,
    EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE = 0x800, /* position of the spawning work record */
    EFFECT_EMITTER_ORIGIN_MAP_CENTER = 0xa00,
    EFFECT_EMITTER_ORIGIN_TRACKED_ENTITY = 0xc00, /* target plus its unit coordinates */
} effect_emitter_origin_e;

enum { EFFECT_EMITTER_ORIGIN_MASK = 0xe00 };

/* Point particles home toward: bits 5-7 of the 16-bit placement flags (byte 0x02 mask
 * 0xe0). 0x00 and 0x20 both leave the
 * interpolated offset unchanged; 0x40 is the map center. */
typedef enum effect_emitter_homing {
    EFFECT_EMITTER_HOMING_OFFSET = 0x00,
    EFFECT_EMITTER_HOMING_OFFSET_ALT = 0x20,
    EFFECT_EMITTER_HOMING_MAP_CENTER = 0x40,
    EFFECT_EMITTER_HOMING_CASTER = 0x60,
    EFFECT_EMITTER_HOMING_TARGET = 0x80,
    EFFECT_EMITTER_HOMING_CURSOR_TILE = 0xa0,
} effect_emitter_homing_e;

enum { EFFECT_EMITTER_HOMING_MASK = 0xe0 };

/* Initial particle velocity mode: the velocity_inward bit (byte 0x06 mask
 * 0x10) and align_to_unit_facing bit (byte 0x07 mask 0x04) of the emitter's
 * 16-bit child/velocity flags. OUTWARD scatters around the base angles, INWARD aims at
 * the spawn point, CASTER_FACING rotates OUTWARD by the caster's facing, and
 * TOWARD_TARGET (battle_effect_spawn_particle_motion) rotates it along the
 * caster-to-target direction. */
typedef enum effect_emitter_velocity {
    EFFECT_EMITTER_VELOCITY_OUTWARD = 0x000,
    EFFECT_EMITTER_VELOCITY_INWARD = 0x010,
    EFFECT_EMITTER_VELOCITY_TOWARD_TARGET = 0x400,
    EFFECT_EMITTER_VELOCITY_CASTER_FACING = 0x410,
} effect_emitter_velocity_e;

enum { EFFECT_EMITTER_VELOCITY_MASK = 0x410 };

/* Bits 0-2 of an effect timeline camera command: the tracks the keyframe moves. */
enum {
    EFFECT_CAMERA_TRACK_ROTATION = 0x1,
    EFFECT_CAMERA_TRACK_POSITION = 0x2,
    EFFECT_CAMERA_TRACK_ZOOM = 0x4,
};

/* The camera table battle_camera_find_active_keyframe found its keyframe in: the high half of the
 * index it reports. */
enum {
    EFFECT_CAMERA_TABLE_PHASE1 = 0,
    EFFECT_CAMERA_TABLE_CHILD = 1,
    EFFECT_CAMERA_TABLE_PHASE2 = 2,
};

/* Bits 0x1e0 of an effect timeline camera command: where the track's target value comes from.
 * The zoom command takes the command's vector for every source but CURRENT and SAVED. */
typedef enum effect_camera_source {
    EFFECT_CAMERA_SOURCE_TARGET = 0x000,           /* the effect target the command names */
    EFFECT_CAMERA_SOURCE_CURRENT_YAW_ROLL = 0x020, /* angle: given pitch; current yaw and roll plus the given */
    EFFECT_CAMERA_SOURCE_GIVEN = 0x040,            /* the command's vector */
    EFFECT_CAMERA_SOURCE_SAVED_PITCH = 0x060,      /* angle: saved pitch, current yaw and roll */
    EFFECT_CAMERA_SOURCE_MAP_CENTER = 0x080,
    EFFECT_CAMERA_SOURCE_CURRENT = 0x0c0,
    EFFECT_CAMERA_SOURCE_SAVED = 0x100,
    EFFECT_CAMERA_SOURCE_CASTER = 0x140,
    EFFECT_CAMERA_SOURCE_TARGET_AVERAGE = 0x180, /* position: the mean of all unit targets */
    EFFECT_CAMERA_SOURCE_CURSOR_TILE = 0x1c0,
} effect_camera_source_e;

enum { EFFECT_CAMERA_SOURCE_MASK = 0x1e0 };

/* Bits 0x1e00 of an effect timeline camera command, kept in g_battle_effect_camera_rotation_mode,
 * _position_mode and _zoom_mode while the move runs: how the track goes from its start to its
 * target over the command's duration. */
typedef enum effect_camera_mode {
    EFFECT_CAMERA_MODE_IDLE = 0x0000,
    EFFECT_CAMERA_MODE_SNAP = 0x0200,
    EFFECT_CAMERA_MODE_COSINE = 0x0400,
    EFFECT_CAMERA_MODE_COSINE_Q12 = 0x0600,
    EFFECT_CAMERA_MODE_LINEAR = 0x0800,
    EFFECT_CAMERA_MODE_ADD_ACCELERATING = 0x0a00, /* add a step that grows from 0 to the target */
    EFFECT_CAMERA_MODE_ADD = 0x0c00,              /* add the target every frame */
    EFFECT_CAMERA_MODE_ADD_DECELERATING = 0x0e00, /* add a step that shrinks from the target to 0 */
    EFFECT_CAMERA_MODE_SHAKE_GROWING = 0x1000,    /* random offsets around the start */
    EFFECT_CAMERA_MODE_SHAKE = 0x1200,
    EFFECT_CAMERA_MODE_SHAKE_DECAYING = 0x1400,
} effect_camera_mode_e;

enum { EFFECT_CAMERA_MODE_MASK = 0x1e00 };

/* Ordering-table depth clamp shared by the EFFECT renderers: a depth below MIN
 * becomes MIN and one at or beyond LIMIT (the OT slot count) becomes MAX. */
enum {
    EFFECT_OT_DEPTH_MIN = 4,
    EFFECT_OT_DEPTH_MAX = 383,
    EFFECT_OT_DEPTH_LIMIT = 384,
    EFFECT_SPRITE_OT_DEPTH_MAX = 382, /* the farthest depth of a particle sprite */
};

/* primitive_type of battle_effect_alloc_primitive: an index into g_battle_effect_primitive_sizes. */
typedef enum effect_primitive_type {
    EFFECT_PRIMITIVE_FT4 = 0,
    EFFECT_PRIMITIVE_DR_MODE = 1,
} effect_primitive_type_e;

/* Size of the per-effect arena used by battle_effect_alloc_primitive. */
enum { EFFECT_PRIMITIVE_BUFFER_SIZE = 32768 };

/* Initial Z coordinate of each particle sprite's four corner vectors. */
enum { EFFECT_CORNER_Z_DEFAULT = 512 };

/* The 16-bit flags at 0x06 of an emitter record (effect_geometry_entry_t.flags), copied to each particle's flags
 * (effect_work_record_t.flags). Bits 0x410 are effect_emitter_velocity_e. A child mode of 1 or 2
 * enables the child emitter; the engine does not tell the two apart. */
enum {
    EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK = 0x0003, /* spawn child_emitter_on_death when the particle dies */
    EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_1 = 0x0001,
    EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_2 = 0x0002,
    EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK = 0x000c, /* spawn child_emitter_mid_life every frame it lives */
    EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_1 = 0x0004,
    EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_2 = 0x0008,
    EFFECT_EMITTER_FLAG_COLOR_CURVES = 0x0040, /* tint by the three color curves; clear, a neutral 128 */
    /* A particle that comes within this distance of its homing target on all three axes ends its
     * life: the field shifted right by SHIFT is 16, 32 or 48 world units, 0 for never. */
    EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_MASK = 0x0300,
    EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_SHIFT = 4,
};

/* Blend mode and primitive transparency control in a geometry record. */
typedef struct effect_texture_page_config {
    u16 blend_mode : 2;
    u16 clear_abe : 1;
    u16 _unused_00 : 13;
} effect_texture_page_config_t;
typedef char assert_effect_texture_page_config_size[sizeof(effect_texture_page_config_t) == 2 ? 1 : -1];

/* texture_page_flags of the effect geometry records. */
enum {
    EFFECT_TEXTURE_PAGE_ABR_MASK = 0x03, /* the semi-transparency rate, shifted into bits 5-6 of the tpage */
    EFFECT_TEXTURE_PAGE_FLAG_CLEAR_ABE = 0x04,
};

/* The 16-bit flags at 0x02 of an emitter record (effect_geometry_entry_t.placement_flags), copied to each
 * particle's kind_flags. Bits 0x0e0 are effect_emitter_homing_e and bits 0xe00
 * effect_emitter_origin_e. The work kind says what a particle's data is: a sprite node placed one
 * of the four EFFECT_LIST_NODE_* ways, or a heap block. Retail emitters use SPRITE_WORLD and
 * SPRITE_WORLD_UNSCALED only. */
enum {
    EFFECT_EMITTER_ORIENT_TO_VELOCITY = 0x0002, /* turn the sprite along its heading on screen */
    EFFECT_EMITTER_BOX_SPREAD = 0x0100,         /* spawn inside the spread box; clear, on the spread ellipsoid */
    EFFECT_WORK_KIND_MASK = 0xf000,
};

/* The work-record payload selected by the high nibble of placement_flags. */
typedef enum effect_work_kind {
    EFFECT_WORK_KIND_SPRITE_WORLD = 0x0000,
    EFFECT_WORK_KIND_SPRITE_SCREEN_OFFSET = 0x1000,
    EFFECT_WORK_KIND_SPRITE_WORLD_UNSCALED = 0x2000,
    EFFECT_WORK_KIND_SPRITE_SCREEN = 0x3000,
    EFFECT_WORK_KIND_HEAP_BLOCK = 0x4000,
} effect_work_kind_e;

/* A particle channel keyframe's 16-bit action value (effect_keyframe_table_t.action), run by
 * battle_effect_run_keyframe_actions when the keyframe is entered. Bit 7 is set in a few retail
 * files and read by nothing. */
enum {
    EFFECT_KEYFRAME_ACTION_CALLBACK_MASK = 0x0007,
    EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY = 0x0008,
    EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY = 0x0010,
    EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY = 0x0020,
    EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION = 0x0040,
    EFFECT_KEYFRAME_ACTION_ANIMATION_MASK = 0xff00,
    EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT = 8,
};

/* An eight-byte prefix followed by one pointer for each sprite part. */
typedef struct effect_sprite_block {
    struct {
        rgb24_t color;
        u8 sprite_count;
    } header;
    u8 _unused_04[4];
    struct effect_sprite_part* frames[0]; /* 0x08; one pointer per sprite part */
} effect_sprite_block_t;
typedef char assert_effect_sprite_block_prefix_size[sizeof(effect_sprite_block_t) == 8 ? 1 : -1];

/* timeline state */
/* Steppers use the named tracks; resets clear each complete slot array.
 * The reset extents include slots that no track stepper reads. */
enum {
    EFFECT_CHILD_TRACK_RESET_SLOT_COUNT = 13,
    EFFECT_PARENT_TRACK_RESET_SLOT_COUNT = 26,
};

/* Child timeline counters: twelve active tracks and one extra reset slot. */
typedef union effect_child_track_counters {
    s16 reset_slots[EFFECT_CHILD_TRACK_RESET_SLOT_COUNT];
    struct {
        s16 particle[EFFECT_PARTICLE_CHANNEL_COUNT];
        s16 sound[EFFECT_SOUND_TRACK_COUNT];
        s16 surroundings;
        s16 caster;
        s16 target;
        s16 screen;
        s16 _unused_18; /* extra 16-bit value cleared by the child reset */
    } tracks;
} effect_child_track_counters_t;

typedef union effect_child_track_steps {
    s16 reset_slots[EFFECT_CHILD_TRACK_RESET_SLOT_COUNT];
    struct {
        u16 particle[EFFECT_PARTICLE_CHANNEL_COUNT]; /* loaded unsigned by opcode 28 */
        u8 _unused_0a[0x10];                         /* remaining slots are reset but not stepped */
    } tracks;
} effect_child_track_steps_t;

typedef struct effect_child_timeline_state {
    effect_child_track_counters_t keyframe;  /* 0x00; record +0x2a */
    effect_child_track_counters_t remaining; /* 0x1a; record +0x44 */
    effect_child_track_steps_t step;         /* 0x34; record +0x5e */
} effect_child_timeline_state_t;
typedef char effect_child_timeline_state_size_must_be_78[sizeof(effect_child_timeline_state_t) == 78 ? 1 : -1];

/* Parent counters: ten particle tracks, fourteen color/sound tracks and
 * two extra 16-bit values. The reset clears all 26, not just the 24 stepped tracks. */
typedef union effect_parent_track_counters {
    s16 reset_slots[EFFECT_PARENT_TRACK_RESET_SLOT_COUNT];
    struct {
        s16 particle[2][EFFECT_PARTICLE_CHANNEL_COUNT];
        s16 surroundings[2];
        s16 caster[2];
        s16 target[2];
        s16 screen[2];
        s16 sound[EFFECT_SOUND_TRACK_COUNT][2];
        s16 _unused_30[2]; /* extra 16-bit values cleared by the parent reset */
    } tracks;
} effect_parent_track_counters_t;

typedef union effect_parent_track_steps {
    s16 reset_slots[EFFECT_PARENT_TRACK_RESET_SLOT_COUNT];
    struct {
        s16 particle[2][EFFECT_PARTICLE_CHANNEL_COUNT];
        u8 _unused_14[0x10]; /* reset only */
        /* The sound tracks' step slots, at the same offset as the counters'
         * sound tracks. Opcode 29 passes the address of each high byte to the
         * sound helper, which never reads it. */
        u8 sound[EFFECT_SOUND_TRACK_COUNT][2][2]; /* record +0xba */
        s16 _unused_30[2];                        /* extra 16-bit values cleared by the parent reset */
    } tracks;
} effect_parent_track_steps_t;

typedef struct effect_parent_timeline_state {
    s16 spawn_countdown;                      /* 0x00; record +0x2a */
    u16 spawned_count;                        /* 0x02; record +0x2c */
    effect_parent_track_counters_t keyframe;  /* 0x04; record +0x2e */
    effect_parent_track_counters_t remaining; /* 0x38; record +0x62 */
    effect_parent_track_steps_t step;         /* 0x6c; record +0x96 */
    u8 _unused_a0[6];                         /* record +0xca..0xcf; untouched by both timeline views */
} effect_parent_timeline_state_t;

/* Opcodes 28 and 29 interpret the same record storage differently. */
typedef union effect_timeline_state {
    effect_child_timeline_state_t child;
    effect_parent_timeline_state_t parent;
} effect_timeline_state_t;
typedef char effect_timeline_state_size_must_be_166[sizeof(effect_timeline_state_t) == 166 ? 1 : -1];

/* The target and frame travel with the counters of the active timeline. */
typedef struct effect_record_timeline {
    s16 target_index;              /* 0x00; index into effect-context targets */
    s16 frame;                     /* 0x02 */
    effect_timeline_state_t state; /* 0x04; opcode 28 child or opcode 29 parent */
} effect_record_timeline_t;
typedef char effect_record_timeline_size_must_be_170[sizeof(effect_record_timeline_t) == 170 ? 1 : -1];

/* effect_list_node_t.kind: how battle_effect_render_particle_sprite places the sprite, and the
 * FRAME_CHANGED bit it sets when the sequence picks another frame. */
enum {
    EFFECT_LIST_NODE_FRAME_CHANGED = 0x1,
    EFFECT_LIST_NODE_PLACEMENT_MASK = 0x6,
};

/* Placement subfield of effect_list_node_t.kind. */
typedef enum effect_list_node_placement {
    EFFECT_LIST_NODE_WORLD = 0x0,          /* projected position, scaled by the camera zoom */
    EFFECT_LIST_NODE_SCREEN_OFFSET = 0x2,  /* screen position from the camera translation, scaled by the zoom */
    EFFECT_LIST_NODE_WORLD_UNSCALED = 0x4, /* projected position, no zoom */
    EFFECT_LIST_NODE_SCREEN = 0x6,         /* screen position as stored, no zoom */
} effect_list_node_placement_e;

/* effect_list_node_t.depth_mode: the third byte of a sequence frame, which adjusts the sprite's
 * ordering-table depth. */
typedef enum effect_sprite_depth_mode {
    EFFECT_SPRITE_DEPTH_UNCHANGED = 0,
    EFFECT_SPRITE_DEPTH_NEARER_8 = 1,
    EFFECT_SPRITE_DEPTH_FIXED_8 = 2,
    EFFECT_SPRITE_DEPTH_FIXED_FARTHEST = 3, /* EFFECT_SPRITE_OT_DEPTH_MAX */
    EFFECT_SPRITE_DEPTH_FIXED_16 = 4,
    EFFECT_SPRITE_DEPTH_NEARER_16 = 5,
} effect_sprite_depth_mode_e;

/* Opcodes in the low seven bits of a sprite-sequence control byte. */
typedef enum effect_sequence_opcode {
    EFFECT_SEQUENCE_OP_LOOP = 1,       /* back to the first byte */
    EFFECT_SEQUENCE_OP_SET_OFFSET = 2, /* two s16 sprite offsets follow */
    EFFECT_SEQUENCE_OP_ADD_OFFSET = 3, /* two s8 sprite offset deltas follow */
} effect_sequence_opcode_e;

/* The high bit distinguishes control bytes from ordinary frame indices. */
enum {
    EFFECT_SEQUENCE_OP_MASK = 0x7f,
    EFFECT_SEQUENCE_OP_FLAG = 0x80,
};

/* View at a sprite-sequence byte cursor: commands occupy one, three or five
 * bytes. The coordinate operands are little-endian bytes without alignment. */
typedef union effect_sequence_instruction {
    struct {
        u8 opcode;
    } header;
    struct {
        u8 frame_index;
        u8 hold;
        u8 depth_mode; /* effect_sprite_depth_mode_e */
    } frame;
    struct {
        u8 opcode;
        u8 x_le[2];
        u8 y_le[2];
    } set_offset;
    struct {
        u8 opcode;
        s8 x;
        s8 y;
    } add_offset;
} effect_sequence_instruction_t;
typedef char effect_sequence_instruction_size_must_be_5[sizeof(effect_sequence_instruction_t) == 5 ? 1 : -1];

/* particle records */
/* 0x24-byte sprite node at 0x801c00a4: the animated sprite of one particle
 * (effect_work_record_t.data). Links are node indexes into that array; node 0
 * is the sentinel. g_battle_effect_active_list_node_head is the list head and
 * g_battle_effect_free_list_node_head the free-list head. */
typedef struct effect_list_node {
    u16 prev;                  /* 0x00 */
    u16 next;                  /* 0x02 */
    u16 index;                 /* 0x04: this node's own array index */
    s16 kind;                  /* 0x06: EFFECT_LIST_NODE_* */
    s16 sprite_offset_x;       /* 0x08: added to x when drawn */
    s16 sprite_offset_y;       /* 0x0a: added to y when drawn */
    s16 x;                     /* 0x0c: from the owning work record's position */
    s16 y;                     /* 0x0e */
    s16 z;                     /* 0x10 */
    s16 screen_rotation_angle; /* 0x12: battle_effect_update_node_from_work stores the velocity heading here */
    u8 depth_mode;             /* 0x14: effect_sprite_depth_mode_e */
    u8 _padding_15;            /* aligns frame_timer */
    s16 frame_timer;           /* 0x16: counts down by 2; the next opcode runs at <= 0 */
    u8* sequence_data;         /* 0x18: base of the u8 animation sequence */
    u16 sequence_offset;       /* 0x1c: byte offset of the next sequence opcode */
    u8 frame_group_offset;     /* 0x1e: byte offset into the frame-group offset table */
    u8 sprite_frame_index;     /* 0x1f: frame id from the sequence opcode */
    effect_sprite_block_t* sprite_block; /* 0x20 */
} effect_list_node_t;

static __inline__ effect_sequence_instruction_t* effect_get_current_sequence_instruction(effect_list_node_t* node) {
    return (effect_sequence_instruction_t*)(node->sequence_data + node->sequence_offset);
}
typedef char assert_effect_list_node_size[sizeof(effect_list_node_t) == 36 ? 1 : -1];

/* Motion state of an effect slot (effect_slot_t +0x98) and of a
 * particle (effect_work_record_t +0x08), stepped once a frame by
 * battle_effect_step_motion and battle_effect_integrate_particle_motion:
 * position gains velocity; velocity keeps (inertia -
 * g_battle_effect_inertia_threshold) / inertia of itself, gains
 * acceleration * ONE / inertia and gains g_battle_effect_gravity scaled by
 * weight (4.12 fixed point); acceleration gains acceleration_step. */
typedef struct effect_motion {
    s16 inertia;                 /* 0x00 */
    s16 weight;                  /* 0x02; scales the gravity vector */
    vec3s32_t position;          /* 0x04; 20.12 fixed */
    vec3s32_t velocity;          /* 0x10 */
    vec3s32_t acceleration;      /* 0x1c */
    vec3s32_t acceleration_step; /* 0x28 */
} effect_motion_t;
typedef char effect_motion_size_must_be_52[(sizeof(effect_motion_t) == 52) ? 1 : -1];

/* 0x58-byte particle on an effect record's work list. The emitter initializes
 * its motion, homing and child-emitter state; the integrator advances it. */
typedef struct effect_work_record {
    struct effect_work_record* prev; /* 0x00 */
    struct effect_work_record* next; /* 0x04 */
    effect_motion_t motion;          /* 0x08 */
    vec3s16_t target;                /* 0x3c */
    s16 lifetime;                    /* 0x42; frames or EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN */
    u8 _unused_44;
    u8 homing_curve_selector;      /* 0x45; zero disables, otherwise curve row + 1 */
    rgb24_t color_curves;          /* 0x46; zero-based RGB animation curve indices */
    u8 _padding_49;                /* aligns homing_strength */
    s16 homing_strength;           /* 0x4a */
    s16 kind_flags;                /* 0x4c; EFFECT_EMITTER_* placement and EFFECT_WORK_KIND_* */
    u16 flags;                     /* 0x4e; EFFECT_EMITTER_FLAG_* */
    s16 curve_frame;               /* 0x50 */
    u8 child_emitter_on_death;     /* 0x52 */
    u8 child_emitter_mid_life;     /* 0x53 */
    struct effect_list_node* data; /* 0x54; sprite node, or heap work for EFFECT_WORK_KIND_HEAP_BLOCK */
} effect_work_record_t;
typedef char assert_effect_work_record_size[sizeof(effect_work_record_t) == 88 ? 1 : -1];

enum { EFFECT_RECORD_FLAG_ALLOCATED = 0x1 };

enum {
    EFFECT_SCRIPT_CHILD_SLOT_COUNT = 4,
    EFFECT_SCRIPT_COUNTER_COUNT = 4,
    EFFECT_RECORD_CALLBACK_SLOT_COUNT = 4,
    EFFECT_STATE_RECORD_CAPACITY = 16, /* record 0 is the list sentinel */
    EFFECT_SPRITE_NODE_CAPACITY = 256, /* node 0 is the list sentinel */
    EFFECT_WORK_RECORD_CAPACITY = 256,
};

/* One running effect script: a 0xf8-byte record of g_battle_effect_state_records.
 * The root record runs the effect file's script from its start; the parent
 * timeline (opcode 29) starts one child record per effect target, and opcode
 * 02 starts a child the script keeps track of. Record 0 is the list sentinel,
 * so an index of 0 means none. */
typedef struct effect_record {
    s16 next_index;   /* 0x00: next record on the allocated or the available list */
    s16 record_index; /* 0x02: this record's array index */
    s16 parent_index; /* 0x04: the record that started this one; 0 for the root */
    s16 pc;           /* 0x06: byte offset of the current instruction in script */
    const u8* script; /* 0x08: borrowed script byte stream, shared with child records */
    s16 child_indices[EFFECT_SCRIPT_CHILD_SLOT_COUNT]; /* 0x0c: child records started by opcode 02 */
    s16 counters[EFFECT_SCRIPT_COUNTER_COUNT];         /* 0x14: selected by the instruction's top two parameter bits */
    s16 work_count;                                    /* 0x1c: particles on work_head */
    u16 flags;                                         /* 0x1e: EFFECT_RECORD_FLAG_ALLOCATED */
    s16 frame_counter; /* 0x20: frames the script has run, 0 on its first; the frame of script-spawned emitters */
    u8 callback_phase[EFFECT_RECORD_CALLBACK_SLOT_COUNT]; /* 0x22: effect_callback_phase_e of each callback handler */
    effect_record_timeline_t timeline;                    /* 0x26; target, frame and timeline counters */
    effect_work_record_t* work_head;                      /* 0xd0: its particles */
    effect_callback_t callbacks[EFFECT_RECORD_CALLBACK_SLOT_COUNT]; /* 0xd4: native callback handlers */
    void* work_slots[EFFECT_RECORD_CALLBACK_SLOT_COUNT];            /* 0xe4: each callback handler's allocated work */
    effect_callback_t last_native_callback; /* 0xf4: callback last called directly by script opcode 07 */
} effect_record_t;

/* pc is a signed byte offset into the shared, 16-bit-aligned script stream. */
static __inline__ const effect_script_instruction_t* effect_get_current_script_instruction(effect_record_t* record) {
    return (const effect_script_instruction_t*)(record->script + record->pc);
}

static __inline__ u16 effect_get_current_script_header(effect_record_t* record) {
    /* Loading the 16-bit value here preserves the retail base/offset register order. */
    return *(const u16*)(record->script + record->pc);
}

typedef char assert_effect_record_size[sizeof(effect_record_t) == 248 ? 1 : -1];

/* One playback step of the animation script referenced by an effect slot:
 * a hold count and the frame id written to the slot. */
typedef struct effect_anim_step {
    u8 hold;  /* 0x00 */
    u8 frame; /* 0x01 */
} effect_anim_step_t;

typedef char effect_anim_step_size_must_be_2[sizeof(effect_anim_step_t) == 2 ? 1 : -1];

typedef struct effect_anim_script {
    s16 count;                  /* 0x00 */
    effect_anim_step_t step[0]; /* 0x02; count steps follow */
} effect_anim_script_t;
typedef char effect_anim_script_prefix_size_must_be_2[sizeof(effect_anim_script_t) == 2 ? 1 : -1];

/* 0xcc-byte effect slots at 0x801b9278, chained through the leading next/prev
 * byte pair; slot 0 is the null sentinel. g_battle_effect_active_slot_head heads the active chain
 * and g_battle_effect_slot_free_head the free chain. RGB and the frame byte
 * are copied together by the billboard renderer; battle_effect_init_trap_animation resets
 * the color and writes the 16-bit values at 0x88 and 0x8e. */

typedef struct effect_slot {
    u8 next;                      /* 0x00 */
    u8 prev;                      /* 0x01 */
    u8 _padding_02[2];            /* aligns modes */
    DR_MODE modes[2][2];          /* 0x04; two draw modes per buffer */
    POLY_FT4 quads[2];            /* 0x34; one quad per buffer */
    rgb24_t color;                /* 0x84 */
    u8 frame;                     /* 0x87 */
    u16 clut;                     /* 0x88 */
    s16 step_index;               /* 0x8a */
    s16 hold;                     /* 0x8c */
    s16 angle;                    /* 0x8e; billboard quad rotation */
    effect_anim_script_t* script; /* 0x90 */
    s16 life;                     /* 0x94: negative runs until the script wraps */
    u8 _padding_96[0x02];         /* aligns motion */
    effect_motion_t motion;       /* 0x98 */
} effect_slot_t;
typedef char assert_effect_slot_size[sizeof(effect_slot_t) == 204 ? 1 : -1];

/* The spawn block at 0x14-0x4b of an emitter record, as the
 * effect_interpolate_emitter_* helpers read it; the native renderers
 * that spawn like the particle emitter embed it. The spread is the ellipsoid
 * or box half-extent around the spawn position and the velocity spread
 * randomizes the base angles. */
/* geometry */
typedef struct effect_emitter_geometry_view {
    vec3s16_t spawn_position_start;      /* 0x14 */
    vec3s16_t spawn_position_end;        /* 0x1a */
    vec3s16_t spawn_spread_start;        /* 0x20 */
    vec3s16_t spawn_spread_end;          /* 0x26 */
    vec3s16_t velocity_base_angle_start; /* 0x2c */
    vec3s16_t velocity_base_angle_end;   /* 0x32 */
    vec3s16_t velocity_spread_start;     /* 0x38 */
    vec3s16_t velocity_spread_end;       /* 0x3e */
    ranges16_t inertia_start;            /* 0x44 */
    ranges16_t inertia_end;              /* 0x48 */
} effect_emitter_geometry_view_t;
typedef char assert_effect_emitter_geometry_view_size[sizeof(effect_emitter_geometry_view_t) == 56 ? 1 : -1];

/* Nibble positions of the zero-based red, green and blue curve indices in a
 * record's color_curves word. The renderers copy the word into a signed local
 * and shift and mask each nibble. A bit-field record would extract with
 * different shifts, which the original instructions rule out. */
enum {
    EFFECT_COLOR_CURVE_RED_SHIFT = 0,
    EFFECT_COLOR_CURVE_GREEN_SHIFT = 4,
    EFFECT_COLOR_CURVE_BLUE_SHIFT = 8,
};

/* One-based bindings for the emitter's initial motion. */
typedef struct effect_emitter_spawn_curve_bindings {
    u32 position : 4;
    u32 spread : 4;
    u32 velocity_angle : 4;
    u32 velocity_spread : 4;
    u32 inertia : 4;
    u32 _unknown_02 : 4;
    u32 weight : 4;
    u32 radial_speed : 4;
} effect_emitter_spawn_curve_bindings_t;
typedef char
    effect_emitter_spawn_curve_bindings_size_must_be_4[sizeof(effect_emitter_spawn_curve_bindings_t) == 4 ? 1 : -1];

/* The same fields as bit positions. The native renderers copy each binding
 * word into a signed local and extract a field as (word >> SHIFT) & 0xf;
 * unsigned copies change the shifts. */
enum {
    EFFECT_SPAWN_CURVE_SPAWN_POSITION_SHIFT = 0,
    EFFECT_SPAWN_CURVE_SPAWN_SPREAD_SHIFT = 4,
    EFFECT_SPAWN_CURVE_VELOCITY_BASE_ANGLE_SHIFT = 8,
    EFFECT_SPAWN_CURVE_VELOCITY_DIRECTION_SPREAD_SHIFT = 12,
    EFFECT_SPAWN_CURVE_RADIAL_SPEED_SHIFT = 28,
};

/* Minimum and maximum components of a particle-motion vector. */
typedef struct effect_xyz_min_max {
    ranges16_t x;
    ranges16_t y;
    ranges16_t z;
} effect_xyz_min_max_t;
typedef char effect_xyz_min_max_size_must_be_12[sizeof(effect_xyz_min_max_t) == 12 ? 1 : -1];

/* Bit positions in the emitter's packed 32-bit motion-curve binding word.
 * Zero disables a curve; other values select its row starting at one. */
enum {
    EFFECT_EMITTER_CURVE_ACCELERATION_SHIFT = 0,
    EFFECT_EMITTER_CURVE_ACCELERATION_STEP_SHIFT = 4,
    EFFECT_EMITTER_CURVE_LIFETIME_SHIFT = 8,
    EFFECT_EMITTER_CURVE_TARGET_OFFSET_SHIFT = 12,
    EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT = 20,
    EFFECT_EMITTER_CURVE_SPAWN_INTERVAL_SHIFT = 24,
    EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT = 28,
    EFFECT_EMITTER_CURVE_HOMING_MOTION_SHIFT = 30,
};

/* One 196-byte record in the effect file's particle-system section.
 * Native EFFECT renderers reuse its slots for their own parameter groups.
 * One-based motion bindings select interpolation between each start/end pair;
 * minimum/maximum pairs bound the random values chosen for each particle. */
typedef struct effect_geometry_entry {
    u8 _unused_00;
    u8 animation_index;  /* 0x01: the sprite sequence its particles play */
    u16 placement_flags; /* 0x02: EFFECT_EMITTER_* placement bits and EFFECT_WORK_KIND_*; the particle's kind_flags */
    u8 animation_frame_group; /* 0x04: frame group of that sequence's frames */
    u8 _padding_05;           /* aligns flags */
    u16 flags;                /* 0x06: EFFECT_EMITTER_FLAG_* and effect_emitter_velocity_e; the particle's flags */
    effect_emitter_spawn_curve_bindings_t spawn_bindings; /* 0x08 */
    u32 motion_curve_selectors; /* 0x0c: acceleration, acceleration step, lifetime, target offset, count and interval;
                                 * bits 28-29 bind homing strength and bits 30-31 bind its curve */
    u32 color_curves;           /* 0x10: zero-based RGB curve indices in the low three nibbles */
    effect_emitter_geometry_view_t spawn;         /* 0x14: initial position, spread, angles and inertia */
    s16 _unknown_4c[2];                           /* 0x4c: lerped to _unknown_50 and discarded */
    s16 _unknown_50[2];                           /* 0x50 */
    ranges16_t weight_start;                      /* 0x54: scales gravity */
    ranges16_t weight_end;                        /* 0x58 */
    ranges16_t radial_speed_start;                /* 0x5c */
    ranges16_t radial_speed_end;                  /* 0x60 */
    effect_xyz_min_max_t acceleration_start;      /* 0x64 */
    effect_xyz_min_max_t acceleration_end;        /* 0x70 */
    effect_xyz_min_max_t acceleration_step_start; /* 0x7c */
    effect_xyz_min_max_t acceleration_step_end;   /* 0x88 */
    ranges16_t lifetime_start;                    /* 0x94: in frames */
    ranges16_t lifetime_end;                      /* 0x98 */
    vec3s16_t target_offset_start; /* 0x9c: homing target, an offset from what effect_emitter_homing_e names */
    vec3s16_t target_offset_end;   /* 0xa2 */
    u8 _unused_a8[0x08];
    s16 particle_count_start;         /* 0xb0: particles per spawn */
    s16 particle_count_end;           /* 0xb2 */
    s16 spawn_interval_start;         /* 0xb4: frames between spawns */
    s16 spawn_interval_end;           /* 0xb6 */
    ranges16_t homing_strength_start; /* 0xb8 */
    ranges16_t homing_strength_end;   /* 0xbc */
    u8 child_emitter_on_death;        /* 0xc0: see EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK */
    u8 child_emitter_mid_life;        /* 0xc1: see EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK */
    u8 _padding_c2[0x02];             /* tail padding to 4-byte alignment */
} effect_geometry_entry_t;
typedef char assert_effect_geometry_entry_size[sizeof(effect_geometry_entry_t) == 196 ? 1 : -1];

/* One-based curve bindings for texture scroll and wave phase rates. */
typedef struct effect_textured_quad_scroll_curves {
    u32 _unused_00 : 4;
    u32 wave_y_phase_speed : 4;
    u32 scroll_angle : 4;
    u32 wave_x_phase_speed : 4;
    u32 _unused_02 : 12;
    u32 scroll_speed : 4;
} effect_textured_quad_scroll_curves_t;
typedef char assert_effect_textured_quad_scroll_curves_size[sizeof(effect_textured_quad_scroll_curves_t) == 4 ? 1 : -1];

/* One curve controls the shape parameters of both waves. */
typedef struct effect_wave_shape_curves {
    u32 shape : 4;
    u32 _unused_00 : 28;
} effect_wave_shape_curves_t;
typedef char assert_effect_wave_shape_curves_size[sizeof(effect_wave_shape_curves_t) == 4 ? 1 : -1];

/* The scrolling textured quad grids' (2x2, 4x4 and 8x8) and wave mesh's
 * view of one emitter record. Both use the scroll angle and speed bindings.
 * The wave mesh also reads vertical and horizontal phase-speed bindings and
 * one shape binding for both waves. Its vertical phase-speed pair occupies
 * the emitter spawn-spread block at 0x20 and is read by that interpolation helper. */
typedef struct effect_wave_parameters {
    s16 period;
    s16 amplitude_period;
    s16 amplitude;
    u8 _unused_06[2];
    s16 phase_step;
    u8 _unused_0a[2];
} effect_wave_parameters_t;
typedef char effect_wave_parameters_size_must_be_12[sizeof(effect_wave_parameters_t) == 12 ? 1 : -1];

typedef struct effect_textured_quad_geometry_view {
    u8 _unused_00[0x06];
    u16 flags;                                          /* 0x06: EFFECT_EMITTER_FLAG_COLOR_CURVES */
    effect_textured_quad_scroll_curves_t scroll_curves; /* 0x08 */
    effect_wave_shape_curves_t wave_curves;             /* 0x0c */
    u32 color_curves;                                   /* 0x10: nibbles 0-2 = r/g/b */
    u16 texture_u;                                      /* 0x14 */
    u16 texture_v;                                      /* 0x16 */
    u8 _unused_18[0x16];
    s16 scroll_angle_start; /* 0x2e */
    u8 _unused_30[0x04];
    s16 scroll_angle_end; /* 0x34 */
    u8 _unused_36[0x02];
    s16 wave_phase_speed_start[2]; /* 0x38 */
    u8 _unused_3c[0x02];
    s16 wave_phase_speed_end[2]; /* 0x3e */
    u8 _unused_42[0x0a];
    effect_texture_page_config_t texture_page_config; /* 0x4c: EFFECT_TEXTURE_PAGE_* */
    s16 brightness_row;                               /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54 */
    u8 _unused_56[0x06];
    s16 scroll_speed_start; /* 0x5c */
    u8 _unused_5e[0x02];
    s16 scroll_speed_end; /* 0x60 */
    u8 _unused_62[0x02];
    effect_wave_parameters_t wave_y_start; /* 0x64 */
    effect_wave_parameters_t wave_y_end;   /* 0x70 */
    effect_wave_parameters_t wave_x_start; /* 0x7c */
    effect_wave_parameters_t wave_x_end;   /* 0x88 */
    u8 _unused_94[0x14];
    s16 texture_width;  /* 0xa8 */
    s16 texture_height; /* 0xaa */
    u8 _unused_ac[0x18];
} effect_textured_quad_geometry_view_t;
typedef char
    assert_effect_textured_quad_geometry_view_size[sizeof(effect_textured_quad_geometry_view_t) == 196 ? 1 : -1];

/* The ribbon binds target spread at bit 4 of motion_curve_selectors, where the
 * emitter binds acceleration step; its other curves keep the emitter's places. */
enum {
    EFFECT_PARTICLE_RIBBON_CURVE_TARGET_SPREAD_SHIFT = 4,
};

/* This renderer's view of one effect_geometry_entry_t. */
typedef struct effect_particle_ribbon_geometry_view {
    u8 _unused_00[0x02];
    u16 placement_flags; /* 0x02 */
    u8 _unused_04[0x02];
    u16 flags;                                        /* 0x06 */
    u32 spawn_curve_bindings;                         /* 0x08 */
    u32 target_curve_bindings;                        /* 0x0c */
    u32 color_curves;                                 /* 0x10 */
    effect_emitter_geometry_view_t emitter;           /* 0x14: read by the effect_interpolate_emitter_* helpers */
    effect_texture_page_config_t texture_page_config; /* 0x4c */
    s16 brightness_row;                               /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54 */
    u8 _unused_56[0x06];
    ranges16_t radial_speed_start; /* 0x5c */
    ranges16_t radial_speed_end;   /* 0x60 */
    s16 texture_u;                 /* 0x64 */
    u8 _unused_66[0x02];
    s16 texture_v; /* 0x68 */
    u8 _unused_6a[0x02];
    s16 frame_count; /* 0x6c */
    s16 frame_delay; /* 0x6e */
    u8 _unused_70[0x0c];
    s16 target_spread_start_x; /* 0x7c */
    u8 _unused_7e[2];
    s16 target_spread_start_y; /* 0x80 */
    u8 _unused_82[2];
    s16 target_spread_start_z; /* 0x84 */
    u8 _unused_86[2];
    s16 target_spread_end_x; /* 0x88 */
    u8 _unused_8a[2];
    s16 target_spread_end_y; /* 0x8c */
    u8 _unused_8e[2];
    s16 target_spread_end_z; /* 0x90 */
    u8 _unused_92[2];
    ranges16_t lifetime_start;     /* 0x94 */
    ranges16_t lifetime_end;       /* 0x98 */
    vec3s16_t target_offset_start; /* 0x9c */
    vec3s16_t target_offset_end;   /* 0xa2 */
    s16 texture_width;             /* 0xa8 */
    s16 texture_height;            /* 0xaa */
    u8 _unused_ac[0x04];
    s16 particle_count_start; /* 0xb0 */
    s16 particle_count_end;   /* 0xb2 */
    s16 spawn_interval_start; /* 0xb4 */
    s16 spawn_interval_end;   /* 0xb6 */
    u8 _unused_b8[0x08];
    u8 child_emitter_on_death; /* 0xc0 */
    u8 child_emitter_mid_life; /* 0xc1 */
    u8 _padding_c2[0x02];      /* tail padding to 4-byte alignment */
} effect_particle_ribbon_geometry_view_t;
typedef char
    assert_effect_particle_ribbon_geometry_view_size[sizeof(effect_particle_ribbon_geometry_view_t) == 196 ? 1 : -1];

/* Polar motion bindings use one-based curve selectors; zero keeps the starting value. */
typedef struct effect_polar_curve_bindings {
    u16 center : 4;
    u16 radius : 4;
    u16 velocity : 4;
    u16 acceleration : 4;
    u16 angle : 4;
    u16 _unused_02 : 8;
    u16 radius_step : 4;
    u16 radius_step_delta : 4;
    u16 scroll : 4;
    u16 _unused_04 : 4;
    u16 angle_step : 4;
    u8 _unused_06;
    u8 angle_step_delta : 4;
    u8 _unused_07 : 4;
} effect_polar_curve_bindings_t;
typedef char effect_polar_curve_bindings_size_must_be_8[sizeof(effect_polar_curve_bindings_t) == 8 ? 1 : -1];

/* The polar renderer gives the particle emitter's parameter slots its own meanings. */
typedef struct effect_polar_mesh_geometry_view {
    u8 _unused_00[0x06];
    u16 flags;                                    /* 0x06: bit 6 = per-channel color curves */
    effect_polar_curve_bindings_t curve_bindings; /* 0x08 */
    u32 color_curves;                             /* 0x10: nibbles 0-2 = r/g/b */
    vec3s16_t center_start; /* 0x14: spawn position, read by battle_effect_interpolate_emitter_spawn_position */
    vec3s16_t center_end;   /* 0x1a */
    s16 radius_start;       /* 0x20: spawn spread x */
    u8 _unused_22[0x04];
    s16 radius_end; /* 0x26 */
    u8 _unused_28[0x04];
    s16 velocity_angle_start; /* 0x2c: velocity base angle x */
    s16 velocity_speed_start; /* 0x2e: velocity base angle y */
    u8 _unused_30[0x02];
    s16 velocity_angle_end; /* 0x32 */
    s16 velocity_speed_end; /* 0x34 */
    u8 _unused_36[0x02];
    s16 acceleration_angle_start; /* 0x38: velocity spread x */
    s16 acceleration_speed_start; /* 0x3a: velocity spread y */
    u8 _unused_3c[0x02];
    s16 acceleration_angle_end; /* 0x3e */
    s16 acceleration_speed_end; /* 0x40 */
    u8 _unused_42[0x02];
    s16 angle_start; /* 0x44: minimum inertia */
    u8 _unused_46[0x02];
    s16 angle_end; /* 0x48 */
    u8 _unused_4a[0x02];
    /* Bit fields change the 16-bit reload and register allocation. */
    u16 texture_page_flags; /* 0x4c: EFFECT_TEXTURE_PAGE_* */
    s16 brightness_row;     /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54 */
    u8 _unused_56[0x06];
    s16 radius_step_start; /* 0x5c: minimum radial speed */
    u8 _unused_5e[0x02];
    s16 radius_step_end; /* 0x60 */
    u8 _unused_62[0x02];
    s16 radius_step_delta_start; /* 0x64: minimum x acceleration */
    u8 _unused_66[0x0a];
    s16 radius_step_delta_end; /* 0x70 */
    u8 _unused_72[0x0a];
    s16 scroll_u_speed_start; /* 0x7c: minimum x acceleration step */
    u8 _unused_7e[0x02];
    s16 scroll_v_speed_start; /* 0x80: minimum y acceleration step */
    u8 _unused_82[0x06];
    s16 scroll_u_speed_end; /* 0x88 */
    u8 _unused_8a[0x02];
    s16 scroll_v_speed_end; /* 0x8c */
    u8 _unused_8e[0x0e];
    s16 angle_step_start; /* 0x9c: target offset x */
    u8 _unused_9e[0x04];
    s16 angle_step_end; /* 0xa2 */
    u8 _unused_a4[0x04];
    s16 texture_width;  /* 0xa8 */
    s16 texture_height; /* 0xaa */
    u8 _unused_ac[0x08];
    s16 angle_step_delta_start; /* 0xb4: spawn interval */
    s16 angle_step_delta_end;   /* 0xb6 */
    u16 texture_u;              /* 0xb8: minimum start homing strength */
    u16 texture_v;              /* 0xba: maximum start homing strength */
    u8 _unused_bc[0x08];
} effect_polar_mesh_geometry_view_t;
typedef char assert_effect_polar_mesh_geometry_view_size[sizeof(effect_polar_mesh_geometry_view_t) == 196 ? 1 : -1];

/* Radial motion bindings use one-based curve selectors; zero keeps the starting value. */
typedef struct effect_radial_motion_curve_bindings {
    u32 origin : 4;
    u32 _unused_00 : 4;
    u32 texture : 4;
    u32 _unused_01 : 16;
    u32 spin : 4;
} effect_radial_motion_curve_bindings_t;

typedef struct effect_radial_shape_curve_bindings {
    u16 growth : 4;
    u16 growth_step : 4;
    u16 _unused_01 : 8;
    u16 radius : 4;
    u16 _unused_02 : 12;
} effect_radial_shape_curve_bindings_t;
typedef char
    effect_radial_motion_curve_bindings_size_must_be_4[sizeof(effect_radial_motion_curve_bindings_t) == 4 ? 1 : -1];
typedef char
    effect_radial_shape_curve_bindings_size_must_be_4[sizeof(effect_radial_shape_curve_bindings_t) == 4 ? 1 : -1];

/* Geometry parameters shared by ring, tube and summon-dome renderers.
 * Dome renderers leave the row-radius terms unread. */
typedef struct effect_radial_mesh_geometry {
    u8 _unused_00[0x02];
    u16 placement_flags; /* 0x02: bits 9-11 select how the origin is placed */
    u8 _unused_04[0x02];
    u16 flags;                                           /* 0x06: bit 6 = per-channel color curves */
    effect_radial_motion_curve_bindings_t motion_curves; /* 0x08 */
    effect_radial_shape_curve_bindings_t shape_curves;   /* 0x0c */
    u32 color_curves;                                    /* 0x10: nibbles 0-2 = r/g/b */
    vec3s16_t origin_start;                              /* 0x14 */
    vec3s16_t origin_end;                                /* 0x1a */
    s16 u_span;                                          /* 0x20 */
    s16 v_span;                                          /* 0x22 */
    effect_texture_page_config_t texture_page_config;    /* 0x24: bits 0-1 blend rate, bit 2 clears ABE */
    u8 _unused_26[0x06];
    s16 u_start;        /* 0x2c */
    s16 v_start;        /* 0x2e */
    u16 v_scroll_speed; /* 0x30 */
    s16 u_end;          /* 0x32 */
    s16 v_end;          /* 0x34 */
    u8 _unused_36[0x06];
    s16 brightness_row; /* 0x3c */
    u8 _unused_3e[0x1e];
    s16 spin_speed_start; /* 0x5c */
    u8 _unused_5e[0x02];
    s16 spin_speed_end; /* 0x60 */
    u8 _unused_62[0x02];
    s16 growth_start; /* 0x64 */
    u8 _unused_66[0x02];
    s16 height_start; /* 0x68 */
    u8 _unused_6a[0x02];
    s16 row_radius_start; /* 0x6c */
    u8 _unused_6e[0x02];
    s16 growth_end; /* 0x70 */
    u8 _unused_72[0x02];
    s16 height_end; /* 0x74 */
    u8 _unused_76[0x02];
    s16 row_radius_end; /* 0x78 */
    u8 _unused_7a[0x02];
    s16 growth_step_start; /* 0x7c */
    u8 _unused_7e[0x02];
    s16 height_step_start; /* 0x80 */
    u8 _unused_82[0x02];
    s16 row_radius_step_start; /* 0x84 */
    u8 _unused_86[0x02];
    s16 growth_step_end; /* 0x88 */
    u8 _unused_8a[0x02];
    s16 height_step_end; /* 0x8c */
    u8 _unused_8e[0x02];
    s16 row_radius_step_end; /* 0x90 */
    u8 _unused_92[0x16];
    s16 radius_start; /* 0xa8 */
    u8 _unused_aa[0x02];
    s16 radius_end; /* 0xac */
    u8 _unused_ae[0x16];
} effect_radial_mesh_geometry_t;
typedef char assert_effect_radial_mesh_geometry_size[sizeof(effect_radial_mesh_geometry_t) == 196 ? 1 : -1];

/* The tentacle reads bits 4 to 15 of motion_curve_selectors as its tip's
 * curves; particle count and spawn interval keep the emitter's places. */
enum {
    EFFECT_TENTACLE_CURVE_TIP_SPREAD_SHIFT = 4,
    EFFECT_TENTACLE_CURVE_TRAVEL_DURATION_SHIFT = 8,
    EFFECT_TENTACLE_CURVE_TIP_OFFSET_SHIFT = 12,
};

/* Segment renderers reuse the particle emitter's spawn parameters;
 * tip spread, travel duration and tip offset describe the moving endpoint. */
typedef struct effect_tentacle_mesh_geometry_view {
    u8 _unused_00[0x02];
    u16 placement_flags; /* 0x02: bits 9-11 place the spawn origin, bits 5-7 the tip */
    u8 _unused_04[0x02];
    u16 flags;                                        /* 0x06: bits 0-1 and 2-3 enable death and mid-life children */
    u32 spawn_curve_bindings;                         /* 0x08 */
    u32 tip_curve_bindings;                           /* 0x0c */
    u32 color_curves;                                 /* 0x10 */
    effect_emitter_geometry_view_t emitter;           /* 0x14: read by the effect_interpolate_emitter_* helpers */
    effect_texture_page_config_t texture_page_config; /* 0x4c */
    s16 brightness_row;                               /* 0x4e */
    u8 _unused_50[0x04];
    s16 ot_depth; /* 0x54: added to each quad's depth */
    u8 _unused_56[0x06];
    ranges16_t radial_speed_start; /* 0x5c */
    ranges16_t radial_speed_end;   /* 0x60 */
    s16 texture_u;                 /* 0x64 */
    u8 _unused_66[0x02];
    s16 texture_v; /* 0x68 */
    u8 _unused_6a[0x02];
    s16 frame_count; /* 0x6c */
    s16 frame_delay; /* 0x6e */
    u8 _unused_70[0x0c];
    s16 tip_spread_start_x; /* 0x7c */
    u8 _unused_7e[0x02];
    s16 tip_spread_start_y; /* 0x80 */
    u8 _unused_82[0x02];
    s16 tip_spread_start_z; /* 0x84 */
    u8 _unused_86[0x02];
    s16 tip_spread_end_x; /* 0x88 */
    u8 _unused_8a[0x02];
    s16 tip_spread_end_y; /* 0x8c */
    u8 _unused_8e[0x02];
    s16 tip_spread_end_z; /* 0x90 */
    u8 _unused_92[0x02];
    ranges16_t lifetime_start;     /* 0x94: the segment's length */
    ranges16_t lifetime_end;       /* 0x98 */
    vec3s16_t target_offset_start; /* 0x9c: the tip, added to the anchor chosen by placement_flags bits 5-7 */
    vec3s16_t target_offset_end;   /* 0xa2 */
    s16 texture_width;             /* 0xa8 */
    s16 texture_height;            /* 0xaa */
    u8 _unused_ac[0x04];
    s16 particle_count_start; /* 0xb0: segments spawned per interval */
    s16 particle_count_end;   /* 0xb2 */
    s16 spawn_interval_start; /* 0xb4 */
    s16 spawn_interval_end;   /* 0xb6 */
    u8 _unused_b8[0x08];
    u8 child_emitter_on_death; /* 0xc0 */
    u8 child_emitter_mid_life; /* 0xc1: spawned at the trailing point each frame */
    u8 _padding_c2[0x02];      /* tail padding to 4-byte alignment */
} effect_tentacle_mesh_geometry_view_t;
typedef char
    assert_effect_tentacle_mesh_geometry_view_size[sizeof(effect_tentacle_mesh_geometry_view_t) == 196 ? 1 : -1];

/* The E454 and E464 particle handlers use the renderer-specific meanings of
 * the emitter record's 0x4c and 0xa8 fields. */
typedef struct effect_particle_vertex_emitter_view {
    u8 _unused_00[0x4c];
    s16 vertex_group; /* 0x4c: selects a vertex in the effect work buffer */
    u8 _unused_4e[0x5a];
    s16 start_frame; /* 0xa8 */
    s16 end_frame;   /* 0xaa */
    u8 _unused_ac[0x18];
} effect_particle_vertex_emitter_view_t;
typedef char
    assert_effect_particle_vertex_emitter_view_size[sizeof(effect_particle_vertex_emitter_view_t) == 196 ? 1 : -1];

/* The existing 0xc4-byte geometry record, interpreted by its owning renderer.
 * The interpolation helpers read the particle-emitter slots shared by these
 * layouts; renderer-specific fields retain their own names. */
typedef union effect_geometry_record {
    effect_geometry_entry_t emitter;
    effect_textured_quad_geometry_view_t textured_quad;
    effect_particle_ribbon_geometry_view_t particle_ribbon;
    effect_polar_mesh_geometry_view_t polar_mesh;
    effect_radial_mesh_geometry_t radial_mesh;
    effect_tentacle_mesh_geometry_view_t tentacle_mesh;
    effect_particle_vertex_emitter_view_t particle_vertex_emitter;
} effect_geometry_record_t;
typedef char assert_effect_geometry_record_size[sizeof(effect_geometry_record_t) == 196 ? 1 : -1];

/* Particle-system section addressed by effect-file 32-bit header value at 0x0c. */
typedef struct effect_geometry_table {
    u16 _unused_00;        /* 2, or 1 in seven shipped files */
    u16 entry_count;       /* 0x02 */
    vec3s32_t gravity;     /* 0x04: copied to g_battle_effect_gravity by effect script opcode 0x27 */
    s32 inertia_threshold; /* 0x10: passed to battle_effect_set_inertia_threshold by opcode 0x27 */
    /* One 0xc4-byte record per emitter or mesh. The slots keep their curve
     * bindings, but each native EFFECT renderer gives them its own meanings,
     * so a record is read through the member of the renderer that owns it. */
    effect_geometry_record_t entries[0];
} effect_geometry_table_t;
typedef char assert_effect_geometry_table_prefix_size[sizeof(effect_geometry_table_t) == 20 ? 1 : -1];

/* curves */
/* Frames in one animation curve: emitter and particle frame counters wrap here. */
enum { EFFECT_ANIMATION_CURVE_FRAMES = 160 };

/* Textured RGB intensity that preserves the texture color. */
enum { EFFECT_COLOR_NEUTRAL = 128 };

/* One interpolation factor per frame; curve indices select emitter and color tracks. */
typedef u8 effect_animation_curve_t[EFFECT_ANIMATION_CURVE_FRAMES];
typedef char assert_effect_animation_curve_size[sizeof(effect_animation_curve_t) == 160 ? 1 : -1];

/* Parent and child timeline pacing curves, two four-bit values per byte.
 * Even frames use the low nibble; pacing values 1-9 are applied. */
typedef struct effect_timing_curves {
    u8 parent_timeline[300]; /* 0x000 */
    u8 child_timeline[300];  /* 0x12c */
} effect_timing_curves_t;
typedef char effect_timing_curves_size_must_be_600[sizeof(effect_timing_curves_t) == 600 ? 1 : -1];

/* The effect file has one count followed by one 160-byte curve per entry. */
typedef struct effect_animation_curve_table {
    u32 count;
    effect_animation_curve_t curves[0];
} effect_animation_curve_table_t;
typedef char effect_animation_curve_table_size_must_be_4[(sizeof(effect_animation_curve_table_t) == 4) ? 1 : -1];

/* Read a curve value for a one-based binding; zero disables the curve and
 * reads as 0. BATTLE's emitter and the effect overlays share this idiom.
 * The decrement-then-compare-with--1 test is the original's: a zero test or a
 * post-decrement changes over 180 functions. The caller's index doubles as a
 * loop temporary, and a do-while wrapper changes register allocation, so use
 * the block as a standalone statement. */
#define EFFECT_READ_CURVE(index, result, binding, frame)                                                               \
    {                                                                                                                  \
        index = (binding);                                                                                             \
        if (--index != -1) {                                                                                           \
            result = g_battle_effect_animation_curve_data->curves[index][frame];                                       \
        } else {                                                                                                       \
            result = 0;                                                                                                \
        }                                                                                                              \
    }

/* loading and playback */
/* Action/reaction ability IDs index directly; event IDs map after those rows. */
extern s16 g_battle_effect_ability_files[];
extern effect_file_header_t* g_battle_effect_data_ptrs[EFFECT_FILE_ID_COUNT];
extern file_extent_t g_battle_effect_disc_entries[EFFECT_FILE_ID_COUNT];

extern const effect_flags_section_t* g_battle_effect_flags_section;
extern s16 g_battle_effect_load_countdown;
extern u8 g_battle_effect_load_state;      /* effect_load_state_e */
extern s32 g_battle_effect_playback_phase; /* effect_playback_phase_e */

void battle_effect_call_play(void);
void battle_effect_cancel_playback_request(void);
s32 battle_effect_get_playback_phase(void);
s32 battle_effect_update_file_load(void);
void battle_effect_init_system(void);
s32 battle_effect_is_item_ability(s32 ability_id);
u32 battle_effect_ability_has_no_file(s32 ability_id);
void battle_effect_play(void);
void battle_effect_replace_flags_0006(struct effect_flag_prefix* object, s32 value);
void battle_effect_reset_subsystems(void);
void battle_effect_set_ability_animation(u32 packed, s32 ability, effect_secondary_init_t* source);
void battle_effect_start_new(s32 ability_id, s32 target_mode, effect_start_target_data_t target);
s32 battle_effect_try_init_data(void);
s32 battle_effect_update_cycle(void);

/* secondary effects */
extern u16 g_battle_effect_secondary_active_tail;
extern s32 g_battle_effect_buffer_index;

extern DVECTOR g_battle_effect_charge_line_direction_offsets[];
extern u8 g_battle_effect_charge_line_fade_levels[];
extern effect_charge_params_t g_battle_effect_charge_line_params[2];
extern struct effect_secondary_data* g_battle_effect_current_secondary;
extern u32 g_battle_effect_current_slot_id;
extern effect_death_poof_state_t g_battle_effect_death_poof_states[];
extern u8 g_battle_effect_ending_secondary_function_id; /* function byte to store; 5 = nothing */
extern u16 g_battle_effect_secondary_free_head;
extern u32* g_battle_effect_polygon_depth_data;
extern effect_secondary_animation_t g_battle_effect_secondary_animations[];
/* Secondary-animation id per charge animation set, indexed by the low 16 bits of the
 * caller's packed argument. */
extern u8 g_battle_effect_secondary_by_charge_animation[];
extern effect_secondary_data_t g_battle_effect_secondary_data[];
extern effect_secondary_callback_t g_battle_effect_secondary_handlers[23];
extern effect_secondary_state_t g_battle_effect_secondary_state;

/* Brightness (out of 128) of each of a summon-charge ring's ten slots, newest point first. */
extern u8 g_battle_effect_summon_ring_brightness[];
extern u8 g_battle_effect_charge_secondary_id; /* id of the charge secondary effect started with the ability; 0 none */
/* Per-group trap emitter records (0x801b8564, stride 0x2e). The secondary-effect
 * handlers spawn while spawn_start_frame <= timer < spawn_end_frame and finish
 * once timer passes spawn_start_frame with no live slots;
 * battle_effect_update_active_slots walks count active slots. The level-up
 * handler seeds group 5's position.y with -8, then lowers it by 3 per frame. */
extern effect_emitter_values_t g_battle_effect_trap_particle_configs[17];

/* Projectile secondary animation of each weapon item type: BOW_ARROW for bows, CROSSBOW_BOLT for
 * crossbows, NONE for the rest. */
extern u8 g_battle_effect_weapon_projectile_animations[];
/* CLUT of the zodiac poof's particles, an index in the weapons row, for each
 * sprite palette (the secondary effect's parameter). */
extern u8 g_battle_effect_zodiac_poof_animations[];

s32 battle_effect_unused_07_secondary_handler(void);
s32 battle_effect_unused_0a_secondary_handler(void);
s32 battle_effect_unused_0b_secondary_handler(void);
s32 battle_effect_unused_0e_secondary_handler(void);
s32 battle_effect_unused_10_secondary_handler(void);
s32 battle_effect_none_secondary_handler(void);
s32 battle_effect_arrow_arc_secondary_handler(void);
s32 battle_effect_equip_break_secondary_handler(void);
s32 battle_effect_fall_dust_secondary_handler(void);
void battle_effect_finalize_secondary(u8 secondary_id);
effect_secondary_phase_e battle_effect_get_secondary_phase(u8 secondary_id);
s32 battle_effect_golem_secondary_handler(void);
s32 battle_effect_init_fall_dust_secondary(effect_secondary_init_t* source, VECTOR* position);
s32 battle_effect_init_projectile_secondary(s32 animation, struct effect_secondary_init* source);
s32 battle_effect_init_secondary(s32 animation_id, s32 parameter, struct effect_secondary_init* source);
s32 battle_effect_init_weapon_hit_secondary(s32 elements, effect_secondary_init_t* source, s32 unused_is_bow);
s32 battle_effect_level_up_secondary_handler(void);
s32 battle_effect_linear_projectile_secondary_handler(void);
s32 battle_effect_null_secondary_handler(void);
s32 battle_effect_red_orbs_secondary_handler(void);
s32 battle_effect_reflect_secondary_handler(void);
void battle_effect_start_weapon_hit_secondary(struct battle_unit* unit, s32 is_bow);
void battle_effect_set_secondary_death_smoke(battle_unit_t* unit);
void battle_effect_set_secondary_fall_dust(struct battle_unit* unit);
void battle_effect_set_secondary_level_up(struct battle_unit* unit);
void battle_effect_set_secondary_splash(struct battle_unit* unit);
void battle_effect_set_secondary_teleport(struct battle_unit* unit);
s32 battle_effect_set_secondary_teleport_by_runtime_id(u32 runtime_id);
void battle_effect_set_secondary_venom_trap(battle_unit_t* unit);
void battle_effect_set_secondary_zodiac_poof(battle_unit_t* unit);
s32 battle_effect_spell_charge_secondary_handler(void);
s32 battle_effect_splash_secondary_handler(void);
void battle_effect_start_action_projectile_secondary(battle_unit_t* unit);
s32 battle_effect_summon_charge_secondary_handler(void);
s32 battle_effect_teleport_secondary_handler(void);
s32 battle_effect_update_death_poof_secondary(s32 unused_arg0, s32 passthrough);
s32 battle_effect_update_secondary_effects(void);
s32 battle_effect_venom_trap_secondary_handler(void);
s32 battle_effect_weapon_hit_secondary_handler(void);
s32 battle_effect_zodiac_poof_secondary_handler(void);

/* targets */
extern effect_secondary_init_t g_battle_effect_context;
extern u8 g_battle_effect_palette_target_runtime_ids[];
/* Tile target of the current effect: emitter origin CURSOR_TILE, homing CURSOR_TILE and camera source
 * CURSOR_TILE. */
extern effect_target_t g_battle_effect_target_tile;

void battle_effect_build_secondary_init(struct battle_unit* unit, struct effect_secondary_init* effect);
void battle_effect_copy_unit_world_coords_to_vector(u32 runtime_id, VECTOR* destination);
void battle_effect_get_world_coords_by_battle_id(
    s32 adjust_for_tile_height, u32 battle_id, effect_secondary_init_t* effect_coordinates, SVECTOR* destination);
void battle_effect_resolve_target_world_coords(
    s32 add_depth, s32 target_index, effect_secondary_init_t* coord_data, SVECTOR* out);

/* elements */
extern u8 g_battle_effect_element_masks[];

s32 battle_effect_get_element_index(s32 elements);

/* projectiles */
extern s32 g_battle_effect_arc_trajectory_height;
extern s16 g_battle_effect_arctan_angle_mod;
extern s16 g_battle_effect_arrow_arc_caster_relative_yaw;
extern s32 g_battle_effect_arrow_arc_distance;
extern s32 g_battle_effect_arrow_arc_distance_step;
extern s32 g_battle_effect_arrow_arc_height;
extern s16 g_battle_effect_arrow_arc_launch_angle;
extern s32 g_battle_effect_arrow_arc_length;
extern u16 g_battle_effect_arrow_arc_yaw;
extern VECTOR g_battle_effect_projectile_delta;
extern s32 g_battle_effect_projectile_length;
extern s32 g_battle_effect_projectile_progress;
extern s32 g_battle_effect_projectile_speed;
extern s16 g_battle_effect_projectile_spin_passes;
extern s16 g_battle_effect_projectile_step_count;
extern SVECTOR g_battle_effect_trajectory_destination;
extern s32 g_battle_effect_trajectory_hit_unit_id;
extern SVECTOR g_battle_effect_trajectory_origin;
extern SVECTOR g_battle_effect_trajectory_position; /* final projectile position */
extern s32 g_battle_effect_trajectory_source_id;
extern VECTOR g_battle_effect_trajectory_step; /* last step's movement */
extern s32 g_battle_effect_trajectory_step_count;
extern map_tile_coordinates_t g_battle_effect_trajectory_tile;

extern s32 g_battle_effect_trajectory_tile_flags;

s32 battle_effect_check_arc_trajectory_between_units(u8 source_id, u8 target_id);
s32 battle_effect_check_direct_trajectory_between_units(u8 source_id, u8 target_id);
s32 battle_effect_check_direct_trajectory_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id);
s32 battle_effect_get_bow_shot_angle(void);
s32 battle_effect_set_and_validate_arc_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);
void battle_effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);

/* models */
extern s32 g_battle_effect_model_command_word_offset;
extern psyq_tmd_packet_t* g_battle_effect_model_commands;
extern psyq_tmd_header_t* g_battle_effect_model_files[];
extern SVECTOR* g_battle_effect_model_normals;
extern psyq_tmd_object_t* g_battle_effect_model_object;
extern SVECTOR* g_battle_effect_model_vertices;
extern MATRIX g_battle_effect_projectile_matrix;

void battle_effect_decode_next_model_command(effect_decoded_model_command_t* out);
s32 battle_effect_init_model_sections(psyq_tmd_header_t* resource, s32 unused_1);

/* timelines */
extern effect_child_timeline_t* g_battle_effect_child_timeline;
extern effect_timeline_t* g_battle_effect_timeline;
extern const effect_timing_curves_t* g_battle_effect_timing_curves;

/* scripts */
/* Written by effect-script opcode 05; no game source reads it. */
extern u16 g_battle_effect_unknown_801bf000;
extern effect_callback_t g_battle_effect_callback_slots[];

extern effect_script_result_e (*g_battle_effect_instructions[120])(effect_record_t*);
extern effect_callback_t g_battle_effect_native_callbacks[EFFECT_NATIVE_CALLBACK_COUNT];
extern const u8* g_battle_effect_script;

effect_script_result_e battle_effect_code_script_04_end(effect_record_t* unused_record);
effect_script_result_e battle_effect_code_script_2e_invalid(effect_record_t* unused_record);

/* particle motion */
extern const effect_animation_curve_table_t* g_battle_effect_animation_curve_data;
extern MATRIX g_battle_effect_emitter_matrix;
extern const effect_geometry_table_t* g_battle_effect_geometry_table;
/* Particle gravity; secondary handlers save it while using (0, ONE, 0). */
extern vec3s32_t g_battle_effect_gravity;
extern s32 g_battle_effect_gravity_modifier;
extern s32 g_battle_effect_gravity_scale;
extern s32 g_battle_effect_inertia_threshold;
/* Vertex work allocated by Banish/Altima and filled by the map renderer. */
extern VECTOR* g_battle_effect_map_vertices;
extern MATRIX g_battle_effect_particle_heading_matrix;
extern vec3s32_t g_battle_effect_saved_gravity;
extern s32 g_battle_effect_saved_inertia_threshold;

void battle_effect_integrate_particle_motion(effect_work_record_t* list);
void battle_effect_interpolate_emitter_spawn_position(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_interpolate_emitter_spawn_spread(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_interpolate_emitter_velocity_base_angles(
    const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_interpolate_emitter_velocity_direction_spread(
    const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_restore_gravity(void);
void battle_effect_restore_inertia_threshold(void);
void battle_effect_save_gravity(void);
void battle_effect_save_inertia_threshold(void);
void battle_effect_set_gravity(const vec3s32_t* src);
void battle_effect_set_inertia_threshold(s32 threshold);
/* parent is the particle a child emitter spawns from, read only for the
 * EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE origin: the particle update passes
 * the particle, the EFFECT overlay's particle handlers a record built on
 * their stack that holds a position, and the timeline none. The emitter index
 * arrives as a full 32-bit value. The function is declared with an int result it
 * never sets and no caller reads. */
s32 battle_effect_spawn_emitter_particles(s16 record_index, s32 frame, s32 emitter_index, effect_work_record_t* parent);
s32 battle_effect_spawn_particle_motion(effect_emitter_values_t* values, effect_motion_t* motion);

/* particle pools */
extern u16 g_battle_effect_active_list_node_head;
extern u8 g_battle_effect_active_slot_head;
/* Unsigned 16-bit indexes: BATTLE list traversal loads these with lhu. */
extern s16 g_battle_effect_allocated_record_head;
extern u16 g_battle_effect_available_record_head;
extern s16 g_battle_effect_current_record_index;
extern u16 g_battle_effect_free_list_node_head;
extern effect_work_record_t* g_battle_effect_free_work_record_head;
extern effect_list_node_t g_battle_effect_list_nodes[EFFECT_SPRITE_NODE_CAPACITY + 1];
extern s16 g_battle_effect_root_record_index;
extern u8 g_battle_effect_slot_free_head;
extern effect_slot_t g_battle_effect_slots[];
extern s32 g_battle_effect_sprite_count;
extern s32 g_battle_effect_sprite_count_peak;
extern effect_record_t g_battle_effect_state_records[EFFECT_STATE_RECORD_CAPACITY + 1];
extern s32 g_battle_effect_work_record_count;
extern s32 g_battle_effect_work_record_peak;
/* Effect work pool: g_battle_effect_work_records is the 256-record allocation, g_battle_effect_free_work_record_head
 * the free-chain head, g_battle_effect_work_record_count the in-use count and g_battle_effect_work_record_peak its
 * peak. */
extern effect_work_record_t* g_battle_effect_work_records;

void battle_effect_free_work_record(s16 record_index, effect_work_record_t* work);
effect_work_record_t* battle_effect_get_list_node(s16 record_index, s16 requested_index);
void battle_effect_init_state_records(void);
s32 battle_effect_update_slot(s32 id);

/* sprite animation */
extern u8* g_battle_effect_animation_sequences;
extern u8* g_battle_effect_animation_sequences_start;
extern u8* g_battle_effect_frame_data;
extern u8* g_battle_effect_frame_data_start;
/* Self-relative animation script table: each 16-bit value is a byte offset from
 * the table's own base to a effect_anim_script_t. Declared as bytes
 * because both uses are byte-based -- the stored pointer is the table's own
 * base plus a byte offset -- and the 16-bit entries are read through a cast
 * at the use site. Declaring it `u16[]` and casting the symbol to `u8*` for
 * the address instead reserves an unused 8-byte frame in
 * battle_effect_init_trap_animation. */
extern u8 g_battle_effect_trap_animation_data[];
extern effect_sprite_section_t g_battle_effect_trap_frame_data;
extern effect_sprite_section_t* g_battle_effect_trap_frame_data_ptr;

/* particle rendering */
/* Four-byte alignment preserves the original 32-bit copies into x/y. */
extern SVECTOR g_battle_effect_corner_bottom_left __attribute__((aligned(4)));
extern SVECTOR g_battle_effect_corner_bottom_right __attribute__((aligned(4)));
extern SVECTOR g_battle_effect_corner_top_left __attribute__((aligned(4)));
extern SVECTOR g_battle_effect_corner_top_right __attribute__((aligned(4)));
extern s32 g_battle_effect_frame_parity;
extern s32 g_battle_effect_frame_start_prim_buffer_offset;
extern MATRIX g_battle_effect_matrix; /* 0x801bc09c */

extern u32* g_battle_effect_otag;
/* Work-kind handlers, indexed by kind_flags >> 12. The int return type is
 * load-bearing even though the result is ignored: the value-returning call is
 * a second set of $2, so sched1 no longer boosts the final `$2 = 1` to the end
 * of its block, and the pc update is allocated $3 as in the target. */
extern s32 (*g_battle_effect_particle_render_handlers[])(effect_record_t* record, effect_work_record_t* work);

extern u8* g_battle_effect_prim_buffer;
extern s32 g_battle_effect_prim_buffer_offset;
extern s32 g_battle_effect_primitive_sizes[3];

s32 battle_effect_get_prim_buffer_offset(void);
void battle_effect_reset_render_state(void);
void battle_effect_start_render_frame(void);

/* camera */
extern VECTOR g_battle_effect_camera_position_current;
extern s32 g_battle_effect_camera_position_duration;
extern s32 g_battle_effect_camera_position_frame;
extern u32 g_battle_effect_camera_position_mode;
extern VECTOR g_battle_effect_camera_position_saved;
extern VECTOR g_battle_effect_camera_position_start;
extern VECTOR g_battle_effect_camera_position_target;
extern SVECTOR g_battle_effect_camera_rotation_current;
extern s32 g_battle_effect_camera_rotation_duration; /* angle track duration */
extern s32
    g_battle_effect_camera_rotation_frame; /* angle track counter; reset by battle_camera_execute_angle_command */
extern u32 g_battle_effect_camera_rotation_mode;
extern SVECTOR g_battle_effect_camera_rotation_saved;
extern SVECTOR g_battle_effect_camera_rotation_start;
extern SVECTOR g_battle_effect_camera_rotation_target;
extern VECTOR g_battle_effect_camera_zoom_current;
extern s32 g_battle_effect_camera_zoom_duration;
extern s32 g_battle_effect_camera_zoom_frame;
extern u32 g_battle_effect_camera_zoom_mode;
extern VECTOR g_battle_effect_camera_zoom_saved;
extern VECTOR g_battle_effect_camera_zoom_start;
extern VECTOR g_battle_effect_camera_zoom_target;

void battle_effect_seed_camera_tracks(void);
void battle_effect_init_camera_and_screen_data(void);

/* palettes */
extern effect_palette_section_t* g_battle_effect_palette_ptr;
/* Six RGB byte triples, packed into both CLUT images as 15-bit colors. */
extern rgb24_t g_battle_effect_reflect_palette_colors[6];
extern rgb24_t g_battle_effect_trap_colors[];

/* sound */
extern u8 g_battle_effect_sound_call_counts[]; /* per-channel call counts */
extern s32 g_battle_effect_sound_data_base;
extern struct suzuki_sound_resource* g_battle_effect_sound_section_ptr;

/* math */
s32 battle_lerp_fixed8(s32 start, s32 end, s32 factor);
void battle_effect_add_random_rotation_offsets(const SVECTOR* source, const SVECTOR* ranges, SVECTOR* destination);
void battle_effect_add_random_vector_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void battle_effect_add_random_vector_offsets_and_store_q12(
    const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void battle_effect_add_rotation_vectors_mod_q12(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void battle_effect_add_svectors(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void battle_effect_add_vector_and_store_q12(const VECTOR* delta, VECTOR* value, VECTOR* out);
void battle_effect_add_vectors(const VECTOR* first, const VECTOR* second, VECTOR* out);
void battle_effect_add_vectors_to_svector(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_270(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_90(const VECTOR* left, const VECTOR* right, SVECTOR* result);
s32 battle_effect_interpolate_s32_cos(s32 start, s32 end, s32 duration, s32 current);
void battle_effect_interpolate_svector_cos(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void battle_effect_interpolate_svector_cos_q12(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void battle_effect_interpolate_svector_linear(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void battle_effect_interpolate_vector_q13_cos(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void battle_effect_interpolate_vector_q13_cos_q12(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void battle_effect_interpolate_vector_q13_linear(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void battle_effect_load_svector(const vec3s16_t* src, SVECTOR* dst);
void battle_effect_load_vector(const vec3s16_t* src, VECTOR* dst);
void battle_effect_mask_vector_low_12_bits(const SVECTOR* src, SVECTOR* dst);
s16 battle_effect_select_larger_magnitude_s16(s16 first, s16 second);
s16 battle_effect_select_largest_magnitude_s16(s16 first, s16 second, s16 third);
void battle_effect_scale_vector_fixed12(const VECTOR* src, VECTOR* dst);
void battle_effect_shift_vector_right_12(const VECTOR* src, VECTOR* dst);

#endif
