#ifndef BATTLE_EFFECT_PARTICLE_H
#define BATTLE_EFFECT_PARTICLE_H

/* BATTLE effect particles: motion, pools, sprite animation and rendering. */

#include "fft/battle_effect.h"

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

/* flags of a sprite part (effect_sprite_part_t, effect_sprite_frameset_t). */
enum {
    EFFECT_SPRITE_PART_CLUT_MASK = 0x000f,
    EFFECT_SPRITE_PART_FLAG_SECOND_CLUT_ROW = 0x0010,  /* the CLUT is of MAP_CLUT_ROW_EFFECT_SECOND */
    EFFECT_SPRITE_PART_TPAGE_MODE_MASK = 0x00e0,       /* the tpage's blend rate and 8-bit color bits */
    EFFECT_SPRITE_PART_FLAG_SEMI_TRANSPARENT = 0x0200, /* draw it semi-transparent */
};

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

/* The particle updater reserves -1 for animation-driven lifetime. */
enum { EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN = -1 };

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

typedef char
    effect_emitter_spawn_curve_bindings_size_must_be_4[sizeof(effect_emitter_spawn_curve_bindings_t) == 4 ? 1 : -1];

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

/* The ribbon binds target spread at bit 4 of motion_curve_selectors, where the
 * emitter binds acceleration step; its other curves keep the emitter's places. */
enum {
    EFFECT_PARTICLE_RIBBON_CURVE_TARGET_SPREAD_SHIFT = 4,
};

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

#endif
