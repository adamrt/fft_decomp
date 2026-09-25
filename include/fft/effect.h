#ifndef FFT_EFFECT_H
#define FFT_EFFECT_H

#include "fft/battle_camera.h"
#include "fft/battle_runtime.h"
#include "fft/main_gfx.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

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

#ifndef FFT_EFFECT_SCRIPT_RESULT_TYPES
#    define FFT_EFFECT_SCRIPT_RESULT_TYPES
typedef enum effect_script_result {
    EFFECT_SCRIPT_RESULT_YIELD = 0,
    EFFECT_SCRIPT_RESULT_CONTINUE = 1,
    EFFECT_SCRIPT_RESULT_ABORT = 2,
} effect_script_result_e;
#endif

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
    u8 _unknown_05[3];                  /* 0x05 */
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

enum {
    BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES = 33,
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
    u16 _padding_06;
} battle_effect_rotation_vector_t;

/* Header followed by one word for each sprite in the effect frame. */
typedef struct battle_effect_sprite_block {
    u8 red;
    u8 green;
    u8 blue;
    u8 sprite_count;
} battle_effect_sprite_block_t;

/* Temporary effect-script workspace cleared by opcode 0x2A. */
typedef struct battle_effect_temp_data {
    u8 _unknown_00[0x26];
    s16 target_index; /* 0x26 */
    s16 frame;        /* 0x28 */
    s16 values_2a[13];
    s16 values_44[13];
    s16 values_5e[13];
} battle_effect_temp_data_t;

typedef char battle_temp_effect_data_size_must_be_0x78[sizeof(battle_effect_temp_data_t) == 0x78 ? 1 : -1];

/* Timed palette changes used by caster and target effect tracks. */
typedef struct battle_effect_palette_track {
    s16 duration[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s8 color[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES][3];
    u8 preset[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} battle_effect_palette_track_t;

typedef char battle_effect_palette_track_size_must_be_0xc8[sizeof(battle_effect_palette_track_t) == 0xC8 ? 1 : -1];

/* Shared layout of the EFFECT/E###.BIN overlay state handlers.
 *
 * Every native overlay routine is a state handler
 *     handler(s16 record_index, s32 byte_offset, s32 entry_index, s32 frame)
 * over one 0xf8-byte record of g_effect_state_records. The state block
 * addressed by byte_offset carries the phase byte (1 init, 2 update, 3 destroy);
 * the record's work-slot table at +0xe4 is indexed by the same byte_offset.
 * Field names are provisional: only offsets proven by matched code are named. */
typedef struct effect_state {
    u8 _unknown_00[0x22];
    u8 phase; /* 0x22: 1 init, 2 update, 3 destroy, 0 idle */
} effect_state_t;

struct effect_list_node;

/* 0x58-byte effect work record. battle_effect_init_record_chain allocates 256
 * of them as one doubly linked free chain; while attached to an effect state
 * record it sits on that record's work list (effect_record_t.work_head). */
typedef struct effect_work_record {
    struct effect_work_record* prev; /* 0x00 */
    struct effect_work_record* next; /* 0x04 */
    u8 _unknown_08[0x04];
    s32 position[3]; /* 0x0c: x/y/z, 20.12 fixed; battle_effect_update_node_from_work copies >> 12 to the node */
    s32 velocity[3]; /* 0x18: x/y/z; battle_effect_update_node_from_work orients the node along it */
    s32 wave_y[3];   /* 0x24: randomized vertical-wave components from the emitter */
    s32 wave_x[3];   /* 0x30: randomized horizontal-wave components from the emitter */
    s16 anchor_x;    /* 0x3c: compared against position x >> 12 */
    s16 anchor_y;    /* 0x3e: compared against position y >> 12 */
    s16 anchor_z;    /* 0x40: compared against position z >> 12 */
    s16 lifetime;    /* 0x42: decremented by the particle update */
    u8 _unknown_44[0x02];
    u8 colour_curves[3]; /* 0x46: r/g/b rows of g_effect_palette_table */
    u8 _unknown_49[0x03];
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
    u8 _unknown_00[0x04];
    struct effect_particle_physics_view* next; /* 0x04 */
    s16 inertia;                               /* 0x08 */
    s16 weight;                                /* 0x0a */
    s32 position[3];                           /* 0x0c */
    s32 velocity[3];                           /* 0x18 */
    s32 acceleration[3];                       /* 0x24 */
    s32 drag[3];                               /* 0x30 */
    s16 target[3];                             /* 0x3c */
    s16 lifetime;                              /* 0x42 */
    u8 _unknown_44;
    u8 homing_curve_index;    /* 0x45 */
    u8 colour_curve_index[3]; /* 0x46 */
    u8 _unknown_49;
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
    u16 next_index;    /* 0x00: next record in the current intrusive list */
    s16 record_index;  /* 0x02: initialized to this record's array index */
    s16 parent_index;  /* 0x04: spawning record (g_battle_effect_current_record_index) for script opcode 02 */
    s16 pc;            /* 0x06: byte offset of the current instruction in script */
    u8* script;        /* 0x08 */
    s16 phase_ids[4];  /* 0x0c: child record indexes stored by script opcode 02 */
    s16 counters[4];   /* 0x14: opcodes 20..23 select one by the argument's top two bits */
    u16 work_count;    /* 0x1c: entries on work_head */
    u16 flags;         /* 0x1e: bit 0 marks an allocated record with a work list */
    s16 emitter_index; /* 0x20: passed by script opcode 26; reset to -1 */
    u8 phase[4];       /* 0x22: per-handler phase, indexed by the handler's byte_offset */
    u8 target_index;   /* 0x26: current target, resolved to an anchor position by the emitter and native handlers */
    u8 _unknown_27;
    s16 timeline_frame; /* 0x28: frame counter of battle_effect_code_script_29_step_parent_timeline; tested by opcodes
                           1d/1e */
    u8 _unknown_2a[0xa6];
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
    u8 _unknown_00[0x26];
    s16 target_index; /* 0x26 */
    u8 _unknown_28[0xd0];
} effect_record_target_view_t;

typedef char assert_effect_record_target_view_size[sizeof(effect_record_target_view_t) == 0xf8 ? 1 : -1];

/* 21-keyframe camera table used by the main and cleanup timelines. */
typedef struct effect_camera_keyframes_21 {
    s16 frame_thresholds[21];
    s16 position[21][3];
    s16 target[21][3];
    s16 zoom[21][3];
    u16 selector[21];
    s16 last_keyframe_index;
} effect_camera_keyframes_21_t;

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
    u8 _unknown_15;
    s16 frame_timer;                            /* 0x16: counts down by 2; the next opcode runs at <= 0 */
    u8* sequence_data;                          /* 0x18: base of the u8 animation sequence */
    s16 sequence_offset;                        /* 0x1c: byte offset of the next sequence opcode */
    s8 frame_group_index;                       /* 0x1e: indexes g_battle_effect_frame_data */
    s8 sprite_frame_index;                      /* 0x1f: frame id from the sequence opcode */
    battle_effect_sprite_block_t* sprite_block; /* 0x20 */
} effect_list_node_t;

typedef char assert_effect_list_node_size[sizeof(effect_list_node_t) == 0x24 ? 1 : -1];

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
    u8 next; /* 0x00 */
    u8 prev; /* 0x01 */
    u8 _unknown_02[2];
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
    u8 _unknown_96[0x02];
    battle_effect_motion_t motion; /* 0x98 */
} battle_effect_slot_t;

typedef char assert_battle_effect_slot_size[sizeof(battle_effect_slot_t) == 0xcc ? 1 : -1];

/* Entry passed to battle_effect_get_timing_curve_value (0x801a1200) and
 * battle_effect_get_animation_timing_curve_value (0x801a1244). The owning
 * record is unidentified; only the halfword at 0x28 is proven. */
typedef struct battle_effect_timing_entry {
    u8 _unknown_00[0x28];
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
    u8 _unknown_18[0x16];
    s16 scroll_angle_start; /* 0x2e */
    u8 _unknown_30[0x04];
    s16 scroll_angle_end; /* 0x34 */
    u8 _unknown_36[0x02];
    s16 wave_phase_speed_start[2]; /* 0x38 */
    u8 _unknown_3c[0x02];
    s16 wave_phase_speed_end[2]; /* 0x3e */
    u8 _unknown_42[0x0a];
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
    u8 _unknown_00;
    u8 animation_index;       /* 0x01: passed to battle_effect_init_parameter_set as the sequence */
    u16 work_kind_flags;      /* 0x02: copied to the work record's kind_flags */
    u8 animation_frame_group; /* 0x04: passed to battle_effect_init_parameter_set, stored doubled at node +0x1e */
    u8 _unknown_05;
    u16 flags;            /* 0x06: copied to the work record's flags; bit 6 = per-channel colour curves */
    u32 curve_indices[2]; /* 0x08 */
    u32 colour_curves;    /* 0x10: nibbles 0-2 = r/g/b */
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
    u8 _unknown_ac[0x04];
    s16 particle_count_start;     /* 0xb0 */
    s16 particle_count_end;       /* 0xb2 */
    s16 spawn_interval_start;     /* 0xb4: frames between spawns */
    s16 spawn_interval_end;       /* 0xb6 */
    s16 homing_strength_start[2]; /* 0xb8: min, max, stored at the work record's +0x4a */
    s16 homing_strength_end[2];   /* 0xbc */
    u8 child_emitter_on_death;    /* 0xc0: used when flags bits 0-1 are set */
    u8 child_emitter_mid_life;    /* 0xc1: used when flags bits 2-3 are set */
    u8 _unknown_c2[0x02];
} effect_geometry_entry_t;

typedef char assert_effect_geometry_entry_size[sizeof(effect_geometry_entry_t) == 0xc4 ? 1 : -1];

/* The E454 and E464 particle handlers use the renderer-specific meanings of
 * the geometry entry's 0x4c and 0xa8 fields. */
typedef struct effect_particle_vertex_emitter_view {
    u8 _unknown_00[0x4c];
    s16 vertex_group; /* 0x4c: selects a vertex in the effect work buffer */
    u8 _unknown_4e[0x5a];
    s16 start_frame; /* 0xa8 */
    s16 end_frame;   /* 0xaa */
    u8 _unknown_ac[0x18];
} effect_particle_vertex_emitter_view_t;

typedef char
    assert_effect_particle_vertex_emitter_view_size[sizeof(effect_particle_vertex_emitter_view_t) == 0xc4 ? 1 : -1];

/* Particle-system section addressed by effect-file header word 0x0c. */
typedef struct effect_geometry_table {
    u16 _unknown_00;       /* 2, or 1 in seven shipped files */
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
    u8 _unknown_00[4];
    u8 factor[0x9c];
} effect_palette_entry_t;

typedef char assert_effect_palette_entry_size[sizeof(effect_palette_entry_t) == 0xa0 ? 1 : -1];

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
extern u8 g_effect_e454_particle_work_offset_scale_16;
extern u8 g_effect_e454_particle_work_offset_scale_64;
extern u8 g_effect_e464_particle_work_offset_scale_16;
extern u8 g_effect_e464_particle_work_offset_scale_64;
extern effect_geometry_table_t* g_effect_geometry_table;

/* Overlay-local vertex brightness rows (12-bit fixed) for the 4x4 grid-mesh
 * family: brightness_row selects the row, the column is the quad row. */
extern u32 g_effect_gfx_brightness_table[][5];
extern effect_palette_entry_t* g_effect_palette_table;
extern u8* g_effect_particle_system_data;
extern effect_record_t g_effect_state_records[];
extern effect_record_target_view_t g_effect_state_records_view[];
extern s16 g_battle_effect_current_record_index;

/* Unnamed data, in address order. */
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

/* effect */
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
extern void battle_effect_interpolate_emitter_spawn_spread(const void* source, s32 factor, s32* out);
extern s32 battle_effect_lerp_linear(s32 start, s32 end, s32 factor);
void battle_effect_play(void);
s32 battle_effect_run_block(s16 index);
s16 battle_effect_select_larger_magnitude_s16(s16 first, s16 second);
s16 battle_effect_select_largest_magnitude_s16(s16 first, s16 second, s16 third);
void battle_effect_set_phase_none(void);
void battle_effect_start_render_frame(void);
s32 battle_effect_update_cycle(void);
s32 battle_effect_update_slot(s32 id);

/* gfx */
void battle_gfx_apply_misc_unit_palette_modulation(
    s32 preset_color, s32 time, s32 misc_id, s32 red, s32 green, s32 blue);
void battle_gfx_start_misc_unit_palette_modulation(
    s32 preset_color, s32 time, s32 misc_id, s32 red, s32 green, s32 blue);

/* stage */
s32 battle_effect_update_stage(void);

/* map */
s32 battle_map_dispatch_map_data_command(s32 command, u32 index, s32 object_state, s32 flag);

/* Low halfwords of a VECTOR's components, as effect_add_scratchpad_xyz_components
 * reads them (lhu at +0, +4 and +8). */
typedef struct effect_wide_vector {
    u16 x;
    u16 pad_x;
    u16 y;
    u16 pad_y;
    u16 z;
} effect_wide_vector_t;

void effect_add_scratchpad_xyz_components(s32 group, s32 point, const VECTOR* offset, SVECTOR* output);

/* Reads curve `nibble` (0 = none, otherwise palette row + 1) at `frame`. The
 * separate decrement keeps the target's compare against -1. */
#define READ_CURVE(result, index, nibble, frame)                                                                       \
    {                                                                                                                  \
        index = (nibble);                                                                                              \
        index--;                                                                                                       \
        if (index != -1) {                                                                                             \
            result = g_effect_palette_table[index].factor[frame];                                                      \
        } else {                                                                                                       \
            result = 0;                                                                                                \
        }                                                                                                              \
    }

/* Scales a scratch record's colour by a 4.12 brightness and packs it with its
 * primitive code into one RGB-code word. */
#define PACK_RGB(scratch, brightness)                                                                                  \
    ((((brightness) * (scratch)->colour[0]) >> 12) | (scratch)->code                                                   \
        | ((((brightness) * (scratch)->colour[1]) >> 12) << 8)                                                         \
        | ((((brightness) * (scratch)->colour[2]) >> 12) << 16))

/* The RGB-code word of vertex `n` of a primitive. */
#define POLY_RGB(poly, n) (*(u32*)&(poly)->r##n)

/* A random value in [-|x|, |x|). */
#define RAND_SPREAD(x) ((x) == -(x) ? (x) : (-(x) < (x) ? rand() % ((x) * 2) - (x) : rand() % (-(x) - (x)) + (x)))

/* A random value between `a` and `b`, in either order. */
#define RAND_BETWEEN(a, b) ((a) == (b) ? (a) : ((b) < (a) ? (b) + rand() % ((a) - (b)) : (a) + rand() % ((b) - (a))))

/* A random value between `min` and `max`, in either order. Spelled with rand()
 * first, unlike RAND_BETWEEN; both spellings occur in the target. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

struct effect_timeline_header;

extern struct effect_timeline_header* g_effect_timeline_channel_base;

void battle_effect_interpolate_emitter_spawn_position(const void* source, s32 factor, s32* out);
void battle_effect_interpolate_emitter_velocity_base_angles(const void* source, s32 factor, s32* out);
void battle_effect_interpolate_emitter_velocity_direction_spread(const void* source, s32 factor, s32* out);

#endif
