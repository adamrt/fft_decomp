/* The effect system, including camera keyframes, effect slots, projectile trajectories, secondary effects and their
 * handlers. */

#include "fft/action.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_geometry.h"
#include "fft/effect_model.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/effect_timeline.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/projectile.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/status.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/stddef.h"
#include "psx/types.h"

enum { EFFECT_SECONDARY_ANIMATION_COUNT = 22 };

/* Four-byte records of g_effect_secondary_animations, indexed by animation id
 * (effect_secondary_animation_id_e); byte 0 selects the
 * g_effect_secondary_handlers handler. */
typedef struct effect_secondary_animation {
    u8 function_id; /* 0x00 */
    u8 _padding_01; /* 0x01; aligns parameter */
    /* 0x02; the projectile handlers: effect_model_e in the low byte, or
     * EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC; the charge handler: its parameter row */
    s16 parameter;
} effect_secondary_animation_t;

/* Maps a camera quadrant index to its map-tile camera-block direction. */
extern u8 g_camera_quadrant_block_directions[4];

/* Secondary animation parameters and particle resources. */
extern effect_secondary_animation_t g_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_COUNT];

/* CLUT of the zodiac poof's particles, an index in the weapons row, for each
 * sprite palette (the secondary effect's parameter). */
extern u8 g_effect_zodiac_poof_animations[];
extern u8 g_effect_element_masks[EFFECT_ELEMENT_INDEX_COUNT];

/* Per-group resident particle emitter records (stride 0x2e). The secondary-effect
 * handlers spawn while spawn_start_frame <= timer < spawn_end_frame and finish
 * once timer passes spawn_start_frame with no live slots;
 * effect_secondary_update_active_slots walks count active slots. The level-up
 * handler seeds group 5's position.y with -8, then lowers it by 3 per frame.
 * Motion bit 0x20 in the level-up and fall-dust rows lies outside the velocity mode mask. */
static effect_emitter_values_t g_effect_secondary_particle_configs[17] = {
    /* 0: weapon hit, first slot set. */
    { 0, 0, 0, 4, 8, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_INWARD, { 0, -16, 0 }, { 12, 9, 12 },
        { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0 }, 0, 0, 3, 1, -1, -1 },
    /* 1: weapon hit, second slot set. */
    { 5, 0, 0, 2, 16, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_TOWARD_TARGET, { -16, 0, 0 },
        { 3, 3, 3 }, { 0, 0, 3840 }, { 1024, 1024, 1536 }, { 1376, 1376 }, -2944, -2960, 8, 1, 12, 6 },
    /* 2: unnamed group 2. */
    { 4, 0, 0, 8, 16, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, 0, 0 },
        { 11, 4, 11 }, { 0, 0, 1280 }, { 0, 4096, 128 }, { -800, -768 }, 2624, 2596, 2, 1, -1, -1 },
    /* 3: zodiac poof. */
    { 10, 0, 0, 7, 24, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, 0, 0 },
        { 8, 9, 8 }, { 2048, 0, 0 }, { 0, 0, 0 }, { -781, -780 }, 1693, 1693, 3, 1, -1, -1 },
    /* 4: death poof. */
    { 12, 0, 0, 1, 2, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_INWARD, { 0, -10, 0 }, { 5, 1, 5 },
        { 2048, 0, 0 }, { 0, 0, 0 }, { -1168, -1200 }, 0, 0, 2, 20, -1, -1 },
    /* 5: level up. */
    { 14, 0, 0, 15, 32, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_INWARD | 0x20, { 0, -124, 0 },
        { 12, 2, 12 }, { 2048, 0, 0 }, { 1696, 1696, 1696 }, { -1184, -1200 }, -1424, -1440, 4, 1, -1, -1 },
    /* 6: fall dust. */
    { 4, 0, 0, 7, 32, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_INWARD | 0x20, { 0, 0, 0 },
        { 11, 4, 11 }, { 2048, 0, 0 }, { 0, 0, 0 }, { -512, -496 }, -672, -672, 2, 1, -1, -1 },
    /* 7: splash. */
    { 6, 0, 0, 7, 32, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, 0, 0 }, { 5, 2, 5 },
        { 2048, 0, 0 }, { 0, 4096, 1280 }, { 1056, 816 }, 1024, 2048, 4, 1, -1, -1 },
    /* 8: trap. */
    { 2, 0, 0, 7, 32, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, -17, 0 },
        { 8, 8, 8 }, { 2048, 0, 0 }, { 1184, 1184, 1184 }, { -528, -496 }, 1044, 1092, 3, 1, 15, 15 },
    /* 9: weapon hit, missed second slot set. */
    { 5, 0, 0, 2, 16, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_TOWARD_TARGET, { -16, 0, 0 },
        { 3, 3, 3 }, { 0, 0, 2304 }, { 1024, 1024, 1536 }, { 1376, 1376 }, -2944, -2960, 8, 1, 12, 6 },
    /* 10: red orbs. */
    { 8, 0, 0, 9, 16, 0, EFFECT_EMITTER_ORIGIN_CASTER, EFFECT_EMITTER_VELOCITY_INWARD, { 0, -18, 0 }, { 32, 32, 32 },
        { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0 }, 1840, 1840, 2, 1, -1, -1 },
    /* 11: equipment break. */
    { 7, 0, 0, 5, 30, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_INWARD, { 0, -24, 0 }, { 4, 2, 4 },
        { 0, 0, 0 }, { 0, 0, 0 }, { 1392, 1424 }, -2384, -2368, 3, 1, -1, -1 },
    /* 12: spell charge. */
    { 5, 0, 0, 8, 10, 0, EFFECT_EMITTER_ORIGIN_CASTER, EFFECT_EMITTER_VELOCITY_INWARD, { 0, -23, 0 }, { 48, 48, 48 },
        { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0 }, 2112, 1872, 1, 1, 10, 10 },
    /* 13: spell charge height copy. */
    { 3, 0, 0, 8, 6, 0, EFFECT_EMITTER_ORIGIN_CASTER, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, -37, 0 },
        { 2, 2, 2 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0 }, 0, 0, 1, 1, 10, 10 },
    /* 14: summon charge. */
    { 9, 0, 0, 8, 30, 0, EFFECT_EMITTER_ORIGIN_CASTER, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, 0, 0 }, { 0, 0, 0 },
        { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0 }, 0, 0, 1, 1, -1, -1 },
    /* 15: teleport. */
    { 13, 0, 0, 48, 30, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, -6, 0 },
        { 15, 17, 15 }, { 2048, 0, 0 }, { 0, 0, 0 }, { 176, 192 }, 2480, 2512, 2, 1, 12, 16 },
    /* 16: Golem. */
    { 16, 0, 0, 1, 1, 0, EFFECT_EMITTER_ORIGIN_TARGET, EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED, { 0, -16, 0 },
        { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0 }, 0, 0, 1, 1, -1, -1 },
};
extern effect_sprite_section_t* g_effect_secondary_particle_frame_data_ptr;

/* Self-relative animation script table: each 16-bit value is a byte offset from
 * the table's own base to a effect_anim_script_t. Declared as bytes
 * because both uses are byte-based -- the stored pointer is the table's own
 * base plus a byte offset -- and the 16-bit entries are read through a cast
 * at the use site. Declaring it `u16[]` and casting the symbol to `u8*` for
 * the address instead reserves an unused 8-byte frame in
 * effect_slot_init_secondary_animation. */
extern u8 g_effect_secondary_particle_animation_data[];

/* Projectile trajectory queries and work state. */
/* Arc endpoint handed to the per-arc obstacle test: the unit's raised tile position followed by its id and height
 * allowance. */
typedef struct projectile_arc_endpoint {
    SVECTOR position;
    s32 unit_id;
    s32 height;
} projectile_arc_endpoint_t;

/* Offset to the target plus its fixed-point horizontal length. */
typedef struct projectile_arc_delta {
    s32 x;
    s32 y;
    s32 z;
    s32 distance;
} projectile_arc_delta_t;

/* Query read by projectile_arc_calculate_trajectory: the offset to the target and both endpoints. */
typedef struct projectile_arc_query {
    projectile_arc_delta_t delta;
    projectile_arc_endpoint_t source;
    projectile_arc_endpoint_t target;
} projectile_arc_query_t;

/* Unit list built in the scratchpad at 0x1f800000 by projectile_list_obstacle_units and read by the obstacle tests. */
typedef struct projectile_obstacle_unit_list {
    s32 count;                           /* 0x00 */
    u8 battle_ids[UNIT_SLOT_COUNT];      /* 0x04 */
    u8 _padding_19;                      /* 0x19; aligns the position vectors */
    SVECTOR positions[UNIT_SLOT_COUNT];  /* 0x1a; tile center and height */
    u8 _padding_c2[2];                   /* 0xc2; aligns the sprite heights */
    s32 sprite_heights[UNIT_SLOT_COUNT]; /* 0xc4 */
} projectile_obstacle_unit_list_t;
typedef char projectile_obstacle_unit_list_size_must_be_280[sizeof(projectile_obstacle_unit_list_t) == 280 ? 1 : -1];

extern s32 g_projectile_gravity_modifier;
extern s32 g_projectile_gravity_scale;
extern s32 g_projectile_launch_slope_q12;
extern s16 g_projectile_arctan_angle_mod;
extern s32 g_projectile_source_id;
extern SVECTOR g_projectile_origin;
extern SVECTOR g_projectile_destination;
extern s32 g_projectile_step_count;
extern VECTOR g_projectile_step;      /* last step's movement */
extern SVECTOR g_projectile_position; /* final projectile position */
extern s32 g_projectile_hit_unit_id;
extern battle_map_tile_coordinates_t g_projectile_tile;
extern s32 g_projectile_tile_flags;

/* 0xc88-byte arrow work buffer of effect_secondary_arrow_arc_handler and
 * effect_secondary_linear_projectile_handler: two sets of four G3 and thirteen G4
 * polygons. */
typedef struct effect_arrow_buffer {
    POLY_G3 g3[2][4];  /* 0x000 */
    POLY_G4 g4[2][13]; /* 0x0e0 */
    u8 _unused_488[0x800];
} effect_arrow_buffer_t;
typedef char effect_arrow_buffer_size_must_be_3208[(sizeof(effect_arrow_buffer_t) == 3208) ? 1 : -1];

/* Double-buffered polygons initialized by the secondary handler and drawn by
 * effect_model_draw. */
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

extern const psyq_tmd_header_t* g_effect_model_files[EFFECT_MODEL_COUNT];

/* Secondary handler work state. */
/* Per-secondary-slot spawn state for the two-particle death poof. */
typedef struct effect_death_poof_state {
    s32 count;        /* 0x00; slots started so far */
    u16 finish_timer; /* 0x04; reaches 1 when the effect is finished */
    u8 ids[2];        /* 0x06 */
} effect_death_poof_state_t;
typedef char effect_death_poof_state_size_must_be_8[sizeof(effect_death_poof_state_t) == 8 ? 1 : -1];

/* One trail of the spell charge effect: the recent screen positions of the effect slot it follows. */
typedef struct effect_charge_trail {
    DVECTOR points[7]; /* 0x00; screen positions, newest at age % 7 */
    s16 age;           /* 0x1c */
    u8 slot_id;        /* 0x1e */
    u8 _padding_1f;    /* tail padding to 2-byte alignment */
} effect_charge_trail_t;

/* Spell-charge state: the center the trails converge on, two buffers of 96 additive lines (16 trails of 6 segments),
 * its particle ids and counts, and the parameters copied from its g_effect_charge_line_params row; color is the
 * charge's element index. */
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

/* g_effect_charge_line_fade_levels holds rows of seven fade steps,
 * indexed flat (row * 7 + step) as the original does. */
enum { EFFECT_CHARGE_LINE_FADE_STEPS = 7 };

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
typedef struct effect_zodiac_poof_state {
    s32 done;   /* 0x00; reaches 1 when the effect is finished */
    s32 count;  /* 0x04; slots started so far */
    u8 ids[24]; /* 0x08 */
} effect_zodiac_poof_state_t;

/* The work area g_effect_secondary_state, shared by the secondary effect handlers; the
 * spell-charge, summon-charge and zodiac-poof handlers each lay it out as
 * their own state. */
typedef union effect_secondary_state {
    effect_charge_state_t charge;
    effect_ring_state_t summon_ring;
    effect_zodiac_poof_state_t zodiac_poof;
} effect_secondary_state_t;

extern effect_secondary_state_t g_effect_secondary_state;
extern effect_death_poof_state_t g_effect_death_poof_states[EFFECT_SECONDARY_RECORD_COUNT];
extern effect_charge_params_t g_effect_charge_line_params[2];

/* Interleaved x/y offsets for eight direction sectors, then the first pair again. */
extern s16 g_effect_charge_line_direction_offsets[18];
extern u8 g_effect_charge_line_fade_levels[];

/* Spell-charge trail color of each effect_element_index_e. */
extern rgb24_t g_effect_charge_trail_colors[EFFECT_ELEMENT_INDEX_COUNT];

/* Brightness (out of 128) of each of a summon-charge ring's ten slots, newest point first. */
extern u8 g_effect_summon_ring_brightness[];

/* Six RGB byte triples, packed into both CLUT images as 15-bit colors. */
extern rgb24_t g_effect_reflect_palette_colors[6];

/* Secondary pool and slot-chain ownership. */
extern effect_secondary_data_t g_effect_secondary_data[EFFECT_SECONDARY_RECORD_COUNT];
extern u16 g_effect_secondary_free_head;
extern u16 g_effect_secondary_active_tail;
extern u32 g_effect_secondary_current_id;
extern effect_secondary_callback_t g_effect_secondary_handlers[23];
extern u8 g_effect_slot_active_head;
extern u8 g_effect_slot_free_head;

/* Effect-camera track clocks. */
extern s32 g_effect_camera_rotation_duration;
extern s32 g_effect_camera_rotation_frame; /* reset by effect_camera_execute_angle_command */
extern s32 g_effect_camera_position_duration;
extern s32 g_effect_camera_position_frame;
extern s32 g_effect_camera_zoom_duration;
extern s32 g_effect_camera_zoom_frame;

/* arrow arc */
/* Flight state of effect_secondary_arrow_arc_handler, set in its first phase from the traced trajectory
 * (g_projectile_*): heading, launch pitch, distance along the arc, steps left and the yaw rotation matrix. */
extern s32 g_effect_arrow_arc_length;
extern u16 g_effect_arrow_arc_yaw;
extern s16 g_effect_arrow_arc_caster_relative_yaw;
extern s32 g_effect_arrow_launch_slope_q12;
extern s16 g_effect_arrow_arc_launch_angle;
extern s32 g_effect_arrow_arc_distance;
extern s32 g_effect_arrow_arc_distance_step;
extern s16 g_effect_projectile_step_count;

/* Provisional: 0x80000000 when an arrow arc ends without a hit; nothing reads it. */
extern s32 g_effect_arrow_miss_marker;

/* straight flight */
/* Flight state of effect_secondary_linear_projectile_handler: the origin-to-destination offset and its length, the
 * progress and speed along it, and the frames left (spin passes). */
extern s32 g_effect_projectile_length;
extern VECTOR g_effect_projectile_delta;
extern s32 g_effect_projectile_progress;
extern s32 g_effect_projectile_speed;
extern s16 g_effect_projectile_spin_passes;

/* World position of a tile's center (x, layer, y in the SVECTOR): x and z are tile * 28 + 14 and y
 * is the tile surface, -(height * BATTLE_MAP_HEIGHT_STEP), without the depth. */
static void effect_convert_tile_coords_to_world_coords(SVECTOR* in, VECTOR* out) {
    battle_map_tile_t* tile = battle_map_tile_get_pointer(in->vx, in->vz, in->vy);

    out->vx = in->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    out->vz = in->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    out->vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
}

/* Copies the camera rotation with its yaw cut to a multiple of 0x200 (an
 * eighth turn) when the tile's camera-block mask leaves the current yaw
 * quadrant open or blocks every direction; otherwise that yaw turned a quarter
 * turn forward or back, whichever quadrant is open first, else a half turn.
 * Only the half-turn result is wrapped to 0-0xfff. The camera angles are read
 * as an SVECTOR: the quadrant is a signed short division, which unsigned
 * 16-bit values do not emit. */
static void effect_camera_choose_unblocked_rotation_for_tile(const SVECTOR* tile_coords, SVECTOR* rotation_out) {
    battle_map_tile_t* tile = battle_map_tile_get_pointer(tile_coords->vx, tile_coords->vz, tile_coords->vy);
    SVECTOR* rotation;
    s32 mask = tile->camera_block_masks & BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    s32 quadrant;
    s32 yaw;

    rotation = camera_rotation_get();
    rotation_out->vx = rotation->vx;
    rotation_out->vz = rotation->vz;
    quadrant = (rotation->vy & 0xfff) / 0x400;
    yaw = rotation->vy & 0xe00;
    if ((g_camera_quadrant_block_directions[quadrant] & mask) == 0
        || mask == BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK) {
        rotation_out->vy = yaw;
        return;
    }
    if ((g_camera_quadrant_block_directions[(quadrant + 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw + 0x400;
    } else if ((g_camera_quadrant_block_directions[(quadrant - 1) & 3] & mask) == 0) {
        rotation_out->vy = yaw - 0x400;
    } else {
        rotation_out->vy = (yaw + 0x800) & 0xfff;
    }
}

/* The 12-byte target stores both arguments to their incoming stack home slots
 * and returns. Taking their addresses reproduces those stores; it does not
 * establish what the original source body contained. */
void effect_camera_noop_1(s32 ignored_1, s32 ignored_2) {
    s32* unused = &ignored_1;
    unused = &ignored_2;
}

/* Writes the map center as the effect camera target: x = max x * 14 and
 * z = max y * 14 (half of a 28-unit tile per tile), with the height (vy) 0. */
static void effect_camera_store_map_center_coordinates(VECTOR* destination) {
    SVECTOR map_max;
    s32 max_y;

    battle_map_store_dimensions(&map_max);
    destination->vx = map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
    max_y = map_max.vz;
    destination->vy = 0;
    destination->vz = max_y * (BATTLE_MAP_TILE_SIZE / 2);
}

/* Timeline track 1 handler: sets the effect camera angle target and duration.
 *
 * Mode bits 0x1e0 pick the source: the best unblocked view of a target's or
 * the caster's tile (0x0, 0x140) or of the cursor tile (0x1c0), the given
 * angles with the yaw wrapped to the nearest turn (0x40), the current
 * rotation (0xc0), the given pitch with offsets added to current yaw/roll (0x20), the
 * saved pitch with the current yaw/roll (0x60) or the saved angles with the yaw
 * wrapped against g_effect_camera_rotation_current (0x100). Most modes then add `angles` when
 * present. Bits 0x1e00 are kept in g_effect_camera_rotation_mode; other modes return untouched.
 *
 * Mode 0x40 keeps its own `current` pointer: sharing `rotation` with the other
 * modes changes that case's register allocation. */
static void effect_camera_execute_angle_command(s32 command, s32 target, s32 duration, const SVECTOR* angles) {
    SVECTOR coords;
    SVECTOR* rotation;
    SVECTOR* current;
    s16 delta;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            unit_get_map_coords_by_runtime_id(g_effect_context.targets[target].data.unit.runtime_id, &coords);
        } else {
            coords = g_effect_context.targets[target].data.tile;
        }
        effect_camera_choose_unblocked_rotation_for_tile(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        current = camera_rotation_get();
        delta = angles->vy - current->vy;
        if (delta < -0x800) {
            delta += ONE;
        } else if (delta > 0x800) {
            delta -= ONE;
        }
        setVector(&g_effect_camera_rotation_target, angles->vx, delta + current->vy, angles->vz);
        g_effect_camera_rotation_mode = command & EFFECT_CAMERA_MODE_MASK;
        g_effect_camera_rotation_duration = duration;
        g_effect_camera_rotation_frame = 0;
        return;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        rotation = camera_rotation_get();
        copyVector(&g_effect_camera_rotation_target, rotation);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_rotation_target = g_effect_camera_rotation_saved;
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        delta = g_effect_camera_rotation_target.vy - g_effect_camera_rotation_current.vy;
        if (delta > 0x800) {
            g_effect_camera_rotation_target.vy -= ONE;
        }
        if (delta < -0x800) {
            g_effect_camera_rotation_target.vy += ONE;
        }
        break;
    case EFFECT_CAMERA_SOURCE_CASTER:
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            unit_get_map_coords_by_runtime_id(
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id, &coords);
        } else {
            coords = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile;
        }
        effect_camera_choose_unblocked_rotation_for_tile(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        coords = g_effect_context.target_tile;
        effect_camera_choose_unblocked_rotation_for_tile(&coords, &g_effect_camera_rotation_target);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT_YAW_ROLL:
        rotation = camera_rotation_get();
        if (angles) {
            setVector(
                &g_effect_camera_rotation_target, angles->vx, rotation->vy + angles->vy, rotation->vz + angles->vz);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED_PITCH:
        rotation = camera_rotation_get();
        setVector(&g_effect_camera_rotation_target, g_effect_camera_rotation_saved.vx, rotation->vy, rotation->vz);
        if (angles) {
            effect_vector_add_svectors(angles, &g_effect_camera_rotation_target, &g_effect_camera_rotation_target);
        }
        break;
    default:
        return;
    }
    g_effect_camera_rotation_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_effect_camera_rotation_duration = duration;
    g_effect_camera_rotation_frame = 0;
}

/* Timeline track 2 handler: sets the effect camera position target and duration.
 *
 * Mode bits 0x1e0 pick the source: effect target `target` (0x0, which also
 * saves the current camera position to g_effect_camera_position_start), the given vector (0x40),
 * the map center (0x80), the current camera (0xc0), the saved position at
 * g_effect_camera_position_saved (0x100), the caster entry (0x140), the average of all unit
 * targets (0x180) or the tile at g_effect_context.target_tile (0x1c0). All but 0x40 add offset
 * when present. Bits 0x1e00 are kept in g_effect_camera_position_mode. */
static void effect_camera_execute_position_command(s32 command, s32 target, s32 duration, const VECTOR* offset) {
    VECTOR sum;
    VECTOR* coords;
    SVECTOR* world;
    s32 i;

    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_copy_unit_world_coords_to_vector(
                g_effect_context.targets[target].data.unit.runtime_id, &g_effect_camera_position_target);
        } else {
            effect_convert_tile_coords_to_world_coords(
                &g_effect_context.targets[target].data.tile, &g_effect_camera_position_target);
        }
        coords = camera_real_coords_get();
        effect_vector_shift_right_12(coords, &g_effect_camera_position_start);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_GIVEN:
        g_effect_camera_position_target = *offset;
        break;
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
        effect_camera_store_map_center_coordinates(&g_effect_camera_position_target);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        coords = camera_real_coords_get();
        setVector(&g_effect_camera_position_target, coords->vx >> 12, coords->vy >> 12, coords->vz >> 12);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_position_target = g_effect_camera_position_saved;
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CASTER:
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_copy_unit_world_coords_to_vector(
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id, &g_effect_camera_position_target);
        } else {
            effect_convert_tile_coords_to_world_coords(
                &g_effect_context.targets[EFFECT_CASTER_ENTRY].data.tile, &g_effect_camera_position_target);
        }
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        /* Retail divides by every target, units or not, and stores the average only
         * when an offset is given; without one the target position is left as it was. */
        setVector(&sum, 0, 0, 0);
        for (i = 0; i < g_effect_context.target_count; i++) {
            if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                world = unit_get_world_coords_by_runtime_id(g_effect_context.targets[i].data.unit.runtime_id);
                sum.vx += world->vx;
                sum.vy += world->vy;
                sum.vz += world->vz;
            }
        }
        sum.vx /= i;
        sum.vy /= i;
        sum.vz /= i;
        if (offset) {
            effect_vector_add(offset, &sum, &g_effect_camera_position_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_CURSOR_TILE:
        effect_convert_tile_coords_to_world_coords(&g_effect_context.target_tile, &g_effect_camera_position_target);
        if (offset) {
            effect_vector_add(offset, &g_effect_camera_position_target, &g_effect_camera_position_target);
        }
        break;
    default:
        return;
    }
    g_effect_camera_position_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_effect_camera_position_duration = duration;
    g_effect_camera_position_frame = 0;
}

/* Timeline track 3 handler: sets the effect camera zoom target and duration.
 *
 * Mode bits 0x1e0 pick the source: the given vector, the current camera zoom
 * (0xc0) or the saved zoom at g_effect_camera_zoom_saved (0x100), the latter two offset by
 * zoom when present. Other modes leave the camera untouched. Bits 0x1e00 are
 * kept in g_effect_camera_zoom_mode. */
static void effect_camera_execute_zoom_command(s32 command, s32 unused, s32 duration, const VECTOR* zoom) {
    switch (command & EFFECT_CAMERA_SOURCE_MASK) {
    case EFFECT_CAMERA_SOURCE_TARGET:
    case EFFECT_CAMERA_SOURCE_GIVEN:
    case EFFECT_CAMERA_SOURCE_MAP_CENTER:
    case EFFECT_CAMERA_SOURCE_CASTER:
    case EFFECT_CAMERA_SOURCE_TARGET_AVERAGE:
        g_effect_camera_zoom_target = *zoom;
        break;
    case EFFECT_CAMERA_SOURCE_CURRENT:
        g_effect_camera_zoom_target = *camera_zoom_get();
        if (zoom) {
            effect_vector_add(zoom, &g_effect_camera_zoom_target, &g_effect_camera_zoom_target);
        }
        break;
    case EFFECT_CAMERA_SOURCE_SAVED:
        g_effect_camera_zoom_target = g_effect_camera_zoom_saved;
        if (zoom) {
            effect_vector_add(zoom, &g_effect_camera_zoom_target, &g_effect_camera_zoom_target);
        }
        break;
    default:
        return;
    }
    g_effect_camera_zoom_mode = command & EFFECT_CAMERA_MODE_MASK;
    g_effect_camera_zoom_duration = duration;
    g_effect_camera_zoom_frame = 0;
}

/* Advance the effect camera rotation, position and zoom tracks one frame.
 *
 * Each track's 32-bit mode (g_effect_camera_rotation_mode, _position_mode and _zoom_mode: bits 0x1e00 of the
 * timeline command) selects an interpolation between the start and target values over the track duration; the mode
 * clears once the counter reaches the duration. */
void effect_camera_update_tracks(void) {
    VECTOR position;
    VECTOR delta;
    SVECTOR rotation;
    SVECTOR rotation_delta;

    switch (g_effect_camera_rotation_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        camera_rotation_set(&g_effect_camera_rotation_target);
        g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_rotation_start.vx = g_effect_camera_rotation_current.vx
            = g_effect_camera_rotation_target.vx & 0xfff;
        g_effect_camera_rotation_start.vy = g_effect_camera_rotation_current.vy
            = g_effect_camera_rotation_target.vy & 0xfff;
        g_effect_camera_rotation_start.vz = g_effect_camera_rotation_current.vz
            = g_effect_camera_rotation_target.vz & 0xfff;
        g_effect_camera_rotation_frame = g_effect_camera_rotation_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_interpolate_svector_cos(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            effect_vector_mask_low_12_bits(&g_effect_camera_rotation_target, &g_effect_camera_rotation_start);
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            camera_rotation_set(&g_effect_camera_rotation_start);
        }
        break;
    /* An out-and-back move ends where it began, so the start stays as it is. */
    case EFFECT_CAMERA_MODE_COSINE_OUT_AND_BACK:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_interpolate_svector_cos_out_and_back(&g_effect_camera_rotation_start,
                &g_effect_camera_rotation_target, g_effect_camera_rotation_duration, g_effect_camera_rotation_frame,
                &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_interpolate_svector_linear(&g_effect_camera_rotation_start, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            effect_vector_mask_low_12_bits(&g_effect_camera_rotation_target, &g_effect_camera_rotation_start);
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
            camera_rotation_set(&g_effect_camera_rotation_start);
        }
        break;
    /* The ADD modes treat the target as a per-frame step: ramped up from zero, constant, or
     * ramped down to zero. At the end the accumulated angle becomes both start and target. */
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&rotation, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_rotations_mod_q12(
                &g_effect_camera_rotation_current, &rotation_delta, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_target = g_effect_camera_rotation_current;
            g_effect_camera_rotation_start = g_effect_camera_rotation_target;
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_vector_add_rotations_mod_q12(
                &g_effect_camera_rotation_current, &g_effect_camera_rotation_target, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_target = g_effect_camera_rotation_current;
            g_effect_camera_rotation_start = g_effect_camera_rotation_target;
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&g_effect_camera_rotation_target, &rotation,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_rotations_mod_q12(
                &g_effect_camera_rotation_current, &rotation_delta, &g_effect_camera_rotation_current);
            camera_rotation_set(&g_effect_camera_rotation_current);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_target = g_effect_camera_rotation_current;
            g_effect_camera_rotation_start = g_effect_camera_rotation_target;
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    /* The SHAKE modes treat the target as a random range around the start angle. */
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&rotation, &g_effect_camera_rotation_target,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_random_rotation_offsets(&g_effect_camera_rotation_start, &rotation_delta, &rotation);
            camera_rotation_set(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            effect_vector_add_random_rotation_offsets(
                &g_effect_camera_rotation_start, &g_effect_camera_rotation_target, &rotation);
            camera_rotation_set(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_rotation_duration > g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            effect_interpolate_svector_linear(&g_effect_camera_rotation_target, &rotation,
                g_effect_camera_rotation_duration, g_effect_camera_rotation_frame, &rotation_delta);
            effect_vector_add_random_rotation_offsets(&g_effect_camera_rotation_start, &rotation_delta, &rotation);
            camera_rotation_set(&rotation);
        }
        if (g_effect_camera_rotation_duration == g_effect_camera_rotation_frame) {
            g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    /* Position track: the same modes on whole-unit values, scaled by ONE for the camera. */
    switch (g_effect_camera_position_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        effect_vector_scale_q12(&g_effect_camera_position_target, &position);
        camera_real_coords_set(&position);
        g_effect_camera_position_current = g_effect_camera_position_target;
        g_effect_camera_position_start = g_effect_camera_position_current;
        g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_position_frame = g_effect_camera_position_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_interpolate_vector_q13_cos(&g_effect_camera_position_start, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &g_effect_camera_position_current);
            effect_vector_scale_q12(&g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_OUT_AND_BACK:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_interpolate_vector_q13_cos_out_and_back(&g_effect_camera_position_start,
                &g_effect_camera_position_target, g_effect_camera_position_duration, g_effect_camera_position_frame,
                &g_effect_camera_position_current);
            effect_vector_scale_q12(&g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_interpolate_vector_q13_linear(&g_effect_camera_position_start, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &g_effect_camera_position_current);
            effect_vector_scale_q12(&g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_and_store_q12(&delta, &g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_target = g_effect_camera_position_current;
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_vector_add_and_store_q12(
                &g_effect_camera_position_target, &g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_target = g_effect_camera_position_current;
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_position_target, &position,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_and_store_q12(&delta, &g_effect_camera_position_current, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_target = g_effect_camera_position_current;
            g_effect_camera_position_start = g_effect_camera_position_target;
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_position_target,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_random_offsets_and_store_q12(&g_effect_camera_position_start, &delta, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            effect_vector_add_random_offsets_and_store_q12(
                &g_effect_camera_position_start, &g_effect_camera_position_target, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_position_duration > g_effect_camera_position_frame) {
            g_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_position_target, &position,
                g_effect_camera_position_duration, g_effect_camera_position_frame, &delta);
            effect_vector_add_random_offsets_and_store_q12(&g_effect_camera_position_start, &delta, &position);
            camera_real_coords_set(&position);
        }
        if (g_effect_camera_position_duration == g_effect_camera_position_frame) {
            g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }

    /* Zoom track: the same modes, with no angle wrap or scaling. */
    switch (g_effect_camera_zoom_mode) {
    case EFFECT_CAMERA_MODE_IDLE:
        break;
    case EFFECT_CAMERA_MODE_SNAP:
        camera_zoom_set(&g_effect_camera_zoom_target);
        g_effect_camera_zoom_current = g_effect_camera_zoom_target;
        g_effect_camera_zoom_start = g_effect_camera_zoom_current;
        g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        g_effect_camera_zoom_frame = g_effect_camera_zoom_duration;
        break;
    case EFFECT_CAMERA_MODE_COSINE:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_interpolate_vector_q13_cos(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_COSINE_OUT_AND_BACK:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_interpolate_vector_q13_cos_out_and_back(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_LINEAR:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_interpolate_vector_q13_linear(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target,
                g_effect_camera_zoom_duration, g_effect_camera_zoom_frame, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_ACCELERATING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_zoom_target, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add(&delta, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_target = g_effect_camera_zoom_current;
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_vector_add(
                &g_effect_camera_zoom_target, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_target = g_effect_camera_zoom_current;
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_ADD_DECELERATING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_zoom_target, &position, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add(&delta, &g_effect_camera_zoom_current, &g_effect_camera_zoom_current);
            camera_zoom_set(&g_effect_camera_zoom_current);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_target = g_effect_camera_zoom_current;
            g_effect_camera_zoom_start = g_effect_camera_zoom_target;
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_GROWING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&position, &g_effect_camera_zoom_target, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add_random_offsets(&g_effect_camera_zoom_start, &delta, &position);
            camera_zoom_set(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            effect_vector_add_random_offsets(&g_effect_camera_zoom_start, &g_effect_camera_zoom_target, &position);
            camera_zoom_set(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    case EFFECT_CAMERA_MODE_SHAKE_DECAYING:
        if (g_effect_camera_zoom_duration > g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            effect_interpolate_vector_q13_linear(&g_effect_camera_zoom_target, &position, g_effect_camera_zoom_duration,
                g_effect_camera_zoom_frame, &delta);
            effect_vector_add_random_offsets(&g_effect_camera_zoom_start, &delta, &position);
            camera_zoom_set(&position);
        }
        if (g_effect_camera_zoom_duration == g_effect_camera_zoom_frame) {
            g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
        }
        break;
    }
}

/* Find the first child-table camera keyframe whose command moves a track in mask and whose end
 * frame is still ahead of frame; report its index and end frame and return 1. Returns 0 at or past
 * the child duration, or when none is left. The single-table form of
 * effect_camera_find_active_keyframe, with no target in the end frame's high half. */
static s32 effect_camera_find_next_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    effect_timeline_t* data = g_effect_timeline;
    s32 i;

    if (frame < data->child.duration) {
        i = 0;
        if (data->last_keyframe_index >= 0) {
            do {
                if (frame < data->end_frame[i] && (mask & data->command[i]) != 0) {
                    *out_index = i;
                    *out_frame = g_effect_timeline->end_frame[i];
                    return 1;
                }
                i++;
                data = g_effect_timeline;
            } while (i <= data->last_keyframe_index);
        }
    }
    return 0;
}

/* Find the first camera keyframe of the tracks in mask that is still ahead
 * of frame, reporting (table << 16 | index) and (target << 16 | end frame).
 *
 * The phase 1 table covers frames before phase1_duration; the child table
 * repeats every spawn_delay frames per effect target (current target, then
 * the next one), and once more for the last target within child_duration; the
 * phase 2 table is relative to the end of that window.
 *
 * The conditional-expression header reads keep the entry block from caching
 * the unit-data pointer. */
static s32 effect_camera_find_active_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    s32 phase1 = g_effect_timeline->phase1_duration;
    s32 spawn_delay;
    s32 child_duration;
    s32 i;
    s32 target_index;
    s32 local_frame;
    s32 elapsed;
    s32 start;

    spawn_delay = g_effect_context.target_count != 0 ? g_effect_timeline->spawn_delay : 0;
    child_duration = g_effect_context.target_count != 0 ? g_effect_timeline->child.duration : 0;

    if (frame < phase1) {
        for (i = 0; i <= g_effect_timeline->phase1_camera.last_keyframe_index; i++) {
            if (frame < g_effect_timeline->phase1_camera.end_frame[i]
                && (mask & g_effect_timeline->phase1_camera.command[i]) != 0) {
                *out_index = i;
                *out_frame = g_effect_timeline->phase1_camera.end_frame[i];
                return 1;
            }
        }
    }

    if (spawn_delay != 0 && frame < phase1 + (g_effect_context.target_count - 1) * spawn_delay) {
        target_index = 0;
        if (frame >= phase1) {
            elapsed = frame - phase1;
            target_index = elapsed / spawn_delay;
            local_frame = elapsed - target_index * spawn_delay;
        } else {
            local_frame = -1;
        }
        for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
            if (g_effect_timeline->end_frame[i] >= spawn_delay) {
                break;
            }
            if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                *out_frame
                    = (target_index << 16) + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                return 1;
            }
        }
        target_index++;
        local_frame = -1;
        if (target_index < g_effect_context.target_count) {
            for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
                if (g_effect_timeline->end_frame[i] >= spawn_delay) {
                    break;
                }
                if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (g_effect_context.target_count != 0) {
        target_index = g_effect_context.target_count - 1;
        start = phase1 + target_index * spawn_delay;
        if (frame < start + child_duration) {
            local_frame = -1;
            if (frame >= start) {
                local_frame = frame - phase1 - target_index * spawn_delay;
            }
            for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
                if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (frame >= phase1 + (g_effect_context.target_count - 1) * spawn_delay + child_duration) {
        local_frame = frame - phase1 - (g_effect_context.target_count - 1) * spawn_delay - child_duration;
    } else {
        local_frame = -1;
    }
    for (i = 0; i <= g_effect_timeline->phase2_camera.last_keyframe_index; i++) {
        if (local_frame < g_effect_timeline->phase2_camera.end_frame[i]
            && (mask & g_effect_timeline->phase2_camera.command[i]) != 0) {
            *out_index = i + (EFFECT_CAMERA_TABLE_PHASE2 << 16);
            *out_frame = g_effect_timeline->phase2_camera.end_frame[i] + phase1
                + (g_effect_context.target_count - 1) * spawn_delay + child_duration;
            return 1;
        }
    }
    return 0;
}

/* Starts the child timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the child camera table. */
void effect_camera_start_child_keyframe_moves(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_camera_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        effect_vector_load_svector(&g_effect_timeline->rotation[index], &rotation);
        effect_camera_execute_angle_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &rotation);
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_camera_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        effect_vector_load(&g_effect_timeline->position[index], &vector);
        effect_camera_execute_position_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_camera_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        effect_vector_load(&g_effect_timeline->zoom[index], &vector);
        effect_camera_execute_zoom_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
}

/* Starts the parent timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the table effect_camera_find_active_keyframe reports (phase 1, child
 * or phase 2). The three-table form of
 * effect_camera_start_child_keyframe_moves. */
void effect_camera_start_parent_keyframe_moves(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;
    s32 k;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_vector_load_svector(&g_effect_timeline->phase1_camera.rotation[k], &rotation);
            effect_camera_execute_angle_command(g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_vector_load_svector(&g_effect_timeline->rotation[k], &rotation);
            effect_camera_execute_angle_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_vector_load_svector(&g_effect_timeline->phase2_camera.rotation[k], &rotation);
            effect_camera_execute_angle_command(g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        }
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_vector_load(&g_effect_timeline->phase1_camera.position[k], &vector);
            effect_camera_execute_position_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_vector_load(&g_effect_timeline->position[k], &vector);
            effect_camera_execute_position_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_vector_load(&g_effect_timeline->phase2_camera.position[k], &vector);
            effect_camera_execute_position_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_vector_load(&g_effect_timeline->phase1_camera.zoom[k], &vector);
            effect_camera_execute_zoom_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_vector_load(&g_effect_timeline->zoom[k], &vector);
            effect_camera_execute_zoom_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_vector_load(&g_effect_timeline->phase2_camera.zoom[k], &vector);
            effect_camera_execute_zoom_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
}

/* Reset the effect system for a battle: the effect slot chains (slot 0 the sentinel, 1-32 free),
 * the 16 secondary effect records (all on the free chain, their slot lists empty), the projectile
 * matrix translation, the render state and the effect camera. */
void effect_init_system(void) {
    s32 i;
    s32 j;

    g_effect_slots[0].prev = 0;
    g_effect_slots[0].next = 0;
    g_effect_slot_free_head = 1;
    for (i = 1; i < EFFECT_SLOT_COUNT; i++) {
        g_effect_slots[i].next = i + 1;
        g_effect_slots[i].prev = i - 1;
    }
    g_effect_slots[i].next = 0;
    g_effect_slots[i].prev = i - 1;
    g_effect_slot_active_head = 0;
    g_effect_projectile_matrix.t[2] = 0;
    g_effect_projectile_matrix.t[1] = 0;
    g_effect_projectile_matrix.t[0] = 0;
    for (i = 1; i < EFFECT_SECONDARY_SLOT_COUNT; i++) {
        g_effect_secondary_data[i].own_slot_id = i;
        g_effect_secondary_data[i].previous_slot_id = i + 1;
        g_effect_secondary_data[i].next_slot_id = i - 1;
        g_effect_secondary_data[i].function_id = 0;
        g_effect_secondary_data[i].allocation = 0;
    }
    g_effect_secondary_data[0].previous_slot_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].next_slot_id = EFFECT_SECONDARY_SLOT_COUNT - 1;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].own_slot_id = EFFECT_SECONDARY_SLOT_COUNT;
    g_effect_secondary_free_head = 1;
    g_effect_secondary_data[0].next_slot_id = 0;
    g_effect_secondary_data[0].own_slot_id = 0;
    g_effect_secondary_data[0].function_id = 0;
    g_effect_secondary_data[0].allocation = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].previous_slot_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].function_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].allocation = 0;
    g_effect_secondary_active_tail = 0;
    g_effect_secondary_particle_frame_data_ptr = &g_effect_secondary_particle_frame_data;
    for (i = 0; i < EFFECT_SECONDARY_RECORD_COUNT; i++) {
        g_effect_secondary_data[i].active_count = 0;
        for (j = EFFECT_SLOT_COUNT - 1; j >= 0; j--) {
            g_effect_secondary_data[i].slot_ids[j] = 0;
        }
    }
    effect_render_reset_state();
    effect_camera_init();
}

/* Take a secondary effect record from the free chain and append it to the active
 * chain, with its timer and slot ids cleared, and return its id. When no record
 * is free it passes code 0x66 to the empty report hook and returns 0.
 *
 * The id is kept as an s32 byte load and a separate `slot = id & 0xff` indexes
 * the table: the target tests and indexes through the masked copy ($v0) while
 * storing and returning the raw id ($s0), which keeps the error path's `move
 * v0,s0` instead of a folded 0. Writing previous_slot_id before the next_slot_id
 * clear reproduces the target's early g_effect_secondary_active_tail load. */
static u8 effect_secondary_allocate_record(void) {
    s32 id = (u8)g_effect_secondary_free_head;
    s32 slot = id & 0xff;
    s32 i;

    if (slot != 0) {
        g_effect_secondary_free_head = g_effect_secondary_data[slot].previous_slot_id;
        g_effect_secondary_data[g_effect_secondary_free_head].next_slot_id = 0;
        g_effect_secondary_data[slot].previous_slot_id = g_effect_secondary_active_tail;
        g_effect_secondary_data[slot].next_slot_id = 0;
        g_effect_secondary_data[g_effect_secondary_active_tail].next_slot_id = id;
        g_effect_secondary_active_tail = id;
        g_effect_secondary_data[slot].timer = 0;
        for (i = 31; i >= 0; i--) {
            g_effect_secondary_data[slot].slot_ids[i] = 0;
        }
        return id;
    }
    system_report_stub(1, 0x66);
    return id;
}

/* Take secondary effect record id (1-16) off the active chain, free its work
 * block and append it to the end of the free chain headed by
 * g_effect_secondary_free_head: the inverse of
 * effect_secondary_allocate_record. An id outside 1-16 only reaches the
 * empty report hook with code 0x67.
 *
 * Both chains are walked through previous_slot_id. A record removed from the
 * middle of the active chain passes its previous link to the record after it, but
 * the record before it keeps next_slot_id pointing at the freed record; only the
 * tail's next link is cleared.
 *
 * The free-chain walk index is an s32: a u16 adds an `andi 0xffff` per load. */
static void effect_secondary_remove(u8 id) {
    effect_secondary_data_t* slot = &g_effect_secondary_data[id];
    s32 tail;

    if (id != 0 && id < EFFECT_SECONDARY_RECORD_COUNT) {
        if (slot->allocation != 0) {
            heap_free(slot->allocation);
            slot->allocation = 0;
        }
        if (slot->next_slot_id != 0) {
            g_effect_secondary_data[slot->next_slot_id].previous_slot_id = slot->previous_slot_id;
        } else {
            g_effect_secondary_active_tail = slot->previous_slot_id;
        }
        g_effect_secondary_data[g_effect_secondary_active_tail].next_slot_id = 0;
        tail = g_effect_secondary_free_head;
        if (tail != 0) {
            while (g_effect_secondary_data[tail].previous_slot_id != 0) {
                tail = g_effect_secondary_data[tail].previous_slot_id;
            }
            g_effect_secondary_data[tail].previous_slot_id = id;
            slot->next_slot_id = tail;
        } else {
            g_effect_secondary_free_head = id;
            slot->next_slot_id = 0;
        }
        slot->previous_slot_id = 0;
        slot->phase = EFFECT_SECONDARY_PHASE_ENDED;
        return;
    }
    system_report_stub(1, 0x67);
}

/* Return the phase of secondary effect record secondary_id; ENDED once the record has been
 * removed. */
effect_secondary_phase_e effect_secondary_get_phase(u8 secondary_id) {
    return g_effect_secondary_data[secondary_id].phase;
}

/* Ask secondary effect secondary_id to wind down: its handler runs the FINALIZING phase next. */
void effect_secondary_finalize(u8 secondary_id) {
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_FINALIZING;
}

/* Take an effect slot from the free chain and push it on the head of the active
 * chain. Returns its id, or 0 (the null sentinel) when no slot is free; the
 * slot's life is cleared either way. */
static s32 effect_slot_alloc(void) {
    u8 head = g_effect_slot_free_head;
    u8 tail;
    s32 id = head;

    if (id != 0) {
        g_effect_slot_free_head = g_effect_slots[id].next;
        g_effect_slots[g_effect_slots[id].next].prev = 0;
        g_effect_slots[g_effect_slot_active_head].prev = head;
        tail = g_effect_slot_active_head;
        g_effect_slots[id].prev = 0;
        g_effect_slots[id].next = tail;
        g_effect_slot_active_head = head;
    }
    g_effect_slots[id].life = 0;
    return id;
}

/* Unlinks slot id from the active chain and pushes it onto the free chain;
 * slot 0 is the null sentinel and is ignored. */
static void effect_slot_free(u8 id) {
    u8 free_head;

    if (id == 0) {
        return;
    }
    g_effect_slots[g_effect_slot_free_head].prev = id;
    if (g_effect_slots[id].prev != 0) {
        g_effect_slots[g_effect_slots[id].prev].next = g_effect_slots[id].next;
    } else {
        g_effect_slot_active_head = g_effect_slots[id].next;
    }
    g_effect_slots[g_effect_slots[id].next].prev = g_effect_slots[id].prev;
    free_head = g_effect_slot_free_head;
    g_effect_slots[id].prev = 0;
    g_effect_slots[id].next = free_head;
    g_effect_slot_free_head = id;
}

/* Start secondary effect animation_id for the caster and first target of the
 * init record and return its id. parameter is the animation's own: an
 * element mask, a sprite palette or a duration. */
s32 effect_secondary_init(s32 animation_id, s32 parameter, const effect_context_t* source) {
    s32 secondary_id = effect_secondary_allocate_record();

    g_effect_secondary_data[secondary_id].animation_id = animation_id;
    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].parameter = parameter;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Start the current effect target's secondary effect: the low half of
 * animation selects the animation id (and its function id), the high half is
 * stored alongside it. Returns its id; phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 effect_secondary_init_projectile(s32 animation, const effect_context_t* source) {
    u8 secondary_id = effect_secondary_allocate_record();

    g_effect_secondary_data[secondary_id].animation_id = animation;
    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].parameter = animation >> 16;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_effect_secondary_data[secondary_id].target_count = source->target_count;
    return secondary_id;
}

/* Returns the effect_element_index_e of an element mask: 0 for no
 * element, otherwise the position of the first table mask it meets (9 when it
 * meets none). */
s32 effect_get_element_index(s32 elements) {
    s32 i;

    if (elements != 0) {
        for (i = 0; i < EFFECT_ELEMENT_INDEX_COUNT; i++) {
            if ((elements & g_effect_element_masks[i]) != 0) {
                break;
            }
        }
        elements = i;
    }
    return elements;
}

/* Start the current effect target's weapon-hit secondary effect.
 *
 * The target's hit result selects the WEAPON_HIT animation (hit, critical,
 * missed, no effect), EQUIP_BREAK (both BREAK results) or GOLEM; REFLECTED
 * keeps the slot's animation. The parameter
 * is the element effect index for the weapon's element mask. Returns its id,
 * or 0 when the init record has no targets. */
s32 effect_secondary_init_weapon_hit(s32 elements, const effect_context_t* source, s32 unused_is_bow) {
    u8 secondary_id;

    if (source->target_count != 0) {
        elements = effect_get_element_index(elements);
        secondary_id = effect_secondary_allocate_record();
        switch (source->targets[0].data.unit.result_animation) {
        case EFFECT_HIT_NORMAL:
        case EFFECT_HIT_CRITICAL:
        case EFFECT_HIT_MISSED:
        case EFFECT_HIT_NO_EFFECT:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_WEAPON_HIT;
            break;
        case EFFECT_HIT_BREAK:
        case EFFECT_HIT_BREAK_ALT:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK;
            break;
        case EFFECT_HIT_GOLEM:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_GOLEM;
            break;
        }
        g_effect_secondary_data[secondary_id].parameter = elements;
        g_effect_secondary_data[secondary_id].function_id
            = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
        g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
        g_effect_secondary_data[secondary_id].target.record = source->targets[0];
        g_effect_secondary_data[secondary_id].timer = 0;
        g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    } else {
        secondary_id = 0;
    }
    return secondary_id;
}

/* Start the current effect target's fall-dust secondary effect at the
 * given position and return its id. Phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 effect_secondary_init_fall_dust(const effect_context_t* source, const VECTOR* position) {
    s32 secondary_id = effect_secondary_allocate_record();

    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_FALL_DUST;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_FALL_DUST]
              .function_id; /* array access keeps the load after the animation store */
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.position.x = position->vx >> 4;
    g_effect_secondary_data[secondary_id].target.position.z = position->vy >> 4;
    g_effect_secondary_data[secondary_id].target.position.y = position->vz >> 4;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Start EFFECT_SECONDARY_ANIMATION_ENDING in a fresh secondary effect
 * record and return its id. Its null handler only steps through the phases, so
 * the record lasts two more frames. The arrow handler starts it when the arrow
 * lands without a hit. */
static s32 effect_secondary_end_on_current_unit(void) {
    s32 secondary_id = effect_secondary_allocate_record();
    s32 record_offset = secondary_id * sizeof(effect_secondary_data_t);
    u8 function_id;

    /* One byte offset serves both stores; indexing the record twice changes the code. */
    *(s16*)((u8*)&g_effect_secondary_data + record_offset + offsetof(effect_secondary_data_t, timer)) = 0;
    *(u16*)((u8*)&g_effect_secondary_data + record_offset + offsetof(effect_secondary_data_t, animation_id))
        = EFFECT_SECONDARY_ANIMATION_ENDING;
    function_id = g_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_ENDING].function_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_effect_secondary_data[secondary_id].function_id = function_id;
    return secondary_id;
}

#define PT(k) (*(VECTOR*)((u8*)scratch + (command.vertices.indices[k] << 4)))

/* Draw one of the models resident in BATTLE.BIN for the current
 * secondary effect: the low byte of parameter is an effect_model_e.
 *
 * The model's vertices go through three transforms in the scratchpad: the spin
 * rotation, then the angles rotation with scale and position, then the camera
 * matrix. The model's draw commands are then decoded one by one into the effect's
 * work buffer for the current frame buffer and linked into the effect ordering
 * table:
 * - ARROW: G4 and G3 commands, at a quarter of the corners' average z (their sum
 *   over 16 or 12).
 * - STONE and SHURIKEN: G3 commands only, at the same depth.
 * - REFLECT: only the first timer * 3 commands, so the shield grows by three
 *   triangles a frame. Its GT3 commands use texture page 0x3f and the CLUT at
 *   0x7eff plus the effect's own slot id. The depth is the last corner's z over
 *   12, and every triangle is linked at ordering table entry 0, not at its
 *   depth.
 *
 * A polygon whose depth is not between 1 and EFFECT_OT_DEPTH_MAX - 1 is built but
 * not drawn. */
static void effect_model_draw(SVECTOR* spin, VECTOR* position, SVECTOR* angles, VECTOR* scale, s32 parameter) {
    effect_decoded_model_command_t command;
    s32 unused[2];
    MATRIX matrix;
    SVECTOR point;
    const psyq_tmd_header_t* model;
    const SVECTOR* vertices;
    VECTOR* scratch;
    s32 count;
    s32 i;
    s32 command_count;
    s32 gt3_index;
    s32 g4_index;
    s32 g3_index;
    s32 j;
    s32 z;
    u32* ot;

    RotMatrix(spin, &matrix);
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    model = g_effect_model_files[parameter & 0xff];
    scratch = (VECTOR*)getScratchAddr(0);
    vertices = (const SVECTOR*)((const u8*)model->objects + model->objects[0].vertex_offset);
    count = model->objects[0].vertex_count;
    for (i = 0; i < count; i++) {
        gte_RotTrans(&vertices[i], &scratch[i], &scratch[i].pad);
    }
    RotMatrix(angles, &matrix);
    ScaleMatrixL(&matrix, scale);
    matrix.t[0] = position->vx;
    matrix.t[1] = position->vy;
    matrix.t[2] = position->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    for (i = 0; i < count; i++) {
        copyVector(&point, &scratch[i]);
        gte_RotTrans(&point, &scratch[i], &scratch[i].pad);
    }
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    for (i = 0; i < count; i++) {
        copyVector(&point, &scratch[i]);
        gte_RotTrans(&point, &scratch[i], &scratch[i].pad);
    }
    command_count = effect_model_init_sections(model, 0);
    gt3_index = 0;
    g4_index = 0;
    g3_index = 0;
    switch (parameter & 0xff) {
    case EFFECT_MODEL_ARROW: {
        effect_arrow_buffer_t* work;
        POLY_G4* quad;
        POLY_G3* tri;

        work = g_effect_secondary_current->allocation;
        for (i = 0; i < command_count;) {
            effect_model_decode_next_command(&command);
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G4) {
                quad = &work->g4[g_effect_buffer_index][g4_index];
                setRGB0(quad, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(quad, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(quad, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                setRGB3(quad, command.colors[3].r, command.colors[3].g, command.colors[3].b);
                quad->x0 = PT(0).vx;
                quad->y0 = PT(0).vy;
                z = PT(0).vz;
                quad->x1 = PT(1).vx;
                quad->y1 = PT(1).vy;
                z += PT(1).vz;
                quad->x2 = PT(2).vx;
                quad->y2 = PT(2).vy;
                z += PT(2).vz;
                quad->x3 = PT(3).vx;
                quad->y3 = PT(3).vy;
                g4_index++;
                z += PT(3).vz;
                z /= 16;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, quad);
                }
            }
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][g3_index];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                g3_index++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_STONE: {
        effect_stone_buffer_t* work;
        POLY_G3* tri;

        work = g_effect_secondary_current->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            effect_model_decode_next_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][j];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                j++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_REFLECT: {
        effect_reflect_buffer_t* work;
        POLY_GT3* tri;

        work = g_effect_secondary_current->allocation;
        /* g4_index is reused here as the limit on decoded commands. */
        g4_index = g_effect_secondary_current->timer * 3;
        if (command_count < g4_index) {
            g4_index = command_count;
        }
        for (i = 0; i < g4_index;) {
            effect_model_decode_next_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_GT3_SEMITRANSPARENT) {
                tri = &work->prims[g_effect_buffer_index][gt3_index];
                tri->tpage = 0x3f;
                tri->clut = g_effect_secondary_current->own_slot_id + 0x7eff;
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                memcpy(&tri->u0, &command.texture.uv[0], sizeof(command.texture.uv[0]));
                memcpy(&tri->u1, &command.texture.uv[1], sizeof(command.texture.uv[1]));
                memcpy(&tri->u2, &command.texture.uv[2], sizeof(command.texture.uv[2]));
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z = PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z = PT(2).vz;
                z /= 12;
                gt3_index++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    ot = g_effect_polygon_depth_data;
                    addPrim(ot, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_SHURIKEN: {
        effect_shuriken_buffer_t* work;
        POLY_G3* tri;

        work = g_effect_secondary_current->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            effect_model_decode_next_command(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][j];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                j++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    }
}

/*
 * Queues the thrown item's icon for the current secondary effect: projects the
 * effect origin to find its OT depth, builds a rotated and camera-scaled 2D
 * matrix from the sprite angle, and transforms the four quad corners into the
 * slot's double-buffered primitive before adding it to the ordering table.
 */
static void effect_draw_thrown_item_sprite(VECTOR* origin, s32 item_id, s32 angle) {
    SPRT sprite;
    SVECTOR local;
    VECTOR position;
    s32 sxy;
    s32 flag;
    effect_slot_t* slot = &g_effect_slots[g_effect_secondary_current->slot_ids[0]];
    POLY_FT4* prim;
    s32 depth;
    s32 sine;
    s32 cosine;
    s32 zoom;
    s32 scale_y;
    s32 half_w;
    s32 half_h;
    s16 right;
    s16 bottom;

    gfx_set_item_icon_sprite(&sprite, item_id);
    prim = &slot->quads[g_effect_buffer_index];
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    copyVector(&local, origin);
    RotTrans(&local, &position, &position.pad);
    depth = (position.vz >> 2) - 8;
    if (depth < EFFECT_OT_DEPTH_MIN) {
        depth = EFFECT_OT_DEPTH_MIN;
    }
    if (depth >= EFFECT_OT_DEPTH_LIMIT) {
        depth = EFFECT_OT_DEPTH_MAX;
    }
    g_effect_matrix.t[0] = position.vx;
    g_effect_matrix.t[1] = position.vy;
    cosine = rcos(angle);
    sine = rsin(angle);
    zoom = g_camera_zoom.vx;
    scale_y = g_camera_zoom.vy;
    g_effect_matrix.m[0][0] = (cosine * zoom) >> 12;
    g_effect_matrix.m[1][1] = (cosine * scale_y) >> 12;
    g_effect_matrix.m[0][1] = (-sine * scale_y) >> 12;
    g_effect_matrix.m[1][0] = (sine * zoom) >> 12;
    SetRotMatrix(&g_effect_matrix);
    SetTransMatrix(&g_effect_matrix);
    half_w = -sprite.w / 2;
    right = half_w + sprite.w;
    half_h = -sprite.h / 2;
    bottom = half_h + sprite.h;
    g_effect_corner_bottom_left.vx = half_w;
    g_effect_corner_top_left.vx = half_w;
    g_effect_corner_bottom_right.vx = right;
    g_effect_corner_top_right.vx = right;
    g_effect_corner_top_right.vy = half_h;
    g_effect_corner_top_left.vy = half_h;
    g_effect_corner_bottom_right.vy = bottom;
    g_effect_corner_bottom_left.vy = bottom;
    RotTransPers3(&g_effect_corner_top_left, &g_effect_corner_top_right, &g_effect_corner_bottom_left, (s32*)&prim->x0,
        (s32*)&prim->x1, (s32*)&prim->x2, &sxy, &flag);
    RotTransPers(&g_effect_corner_bottom_right, (s32*)&prim->x3, &sxy, &flag);
    AddPrim(g_effect_polygon_depth_data + depth, prim);
}

/* Quadratic height term used by the projectile trajectory calculation;
 * this is not the complete trajectory.
 * distance is the attacker-to-target distance scaled by ONE. */
static s32 projectile_arc_calculate_height_term(s32 distance) {
    s32 gravity;

    distance >>= 6;
    gravity = g_projectile_gravity_modifier;
    return gravity / 2 - (distance * distance) / (gravity >> 11);
}

/* Solve the two launch slopes of an arcing projectile toward the target.
 *
 * Tries aim offsets of half, three quarters, then zero of the target's
 * sprite height until both the arc apex and the target tile floor clear it,
 * storing the chosen height back into the target endpoint. Returns 0 when the
 * target is at zero distance or no height clears. */
static s32 projectile_arc_calculate_trajectory(projectile_arc_query_t* query, s32* arc_a, s32* arc_b) {
    battle_map_tile_t* tile;
    s32 tile_height;
    s32 floor;
    s32 peak;
    s32 height;
    s32 limit;
    s32 root;
    s32 gravity;
    s32 distance;
    s32 result = 0;

    tile = battle_map_get_tile_data_ptr_from_battle_id(query->target.unit_id);
    tile_height = tile->height + tile->depth_half_height.depth;
    /* The tile surface, tile_height * BATTLE_MAP_HEIGHT_STEP, in 20.12. */
    floor = (tile_height * 3) << 14;
    peak = projectile_arc_calculate_height_term(query->delta.distance);
    if (query->delta.distance != 0) {
        height = query->target.height / 2;
        limit = (height + query->delta.y) * ONE;
        if (peak >= limit && floor >= limit) {
            query->target.height = height;
        } else {
            height = query->target.height * 3 / 4;
            limit = (height + query->delta.y) * ONE;
            if (peak >= limit && floor >= limit) {
                query->target.height = height;
            } else {
                limit = query->delta.y * ONE;
                if (peak < limit || floor < limit) {
                    return result;
                }
                query->target.height = 0;
            }
        }
        gravity = g_projectile_gravity_modifier >> 6;
        distance = query->delta.distance >> 6;
        root = SquareRoot12((gravity - (limit >> 5)) * gravity - distance * distance);
        *arc_a = ((root + g_projectile_gravity_modifier) << 8) / (query->delta.distance >> 4);
        *arc_b = ((g_projectile_gravity_modifier - root) << 8) / (query->delta.distance >> 4);
        result = 1;
    }
    return result;
}

/*
 * Evaluate the arcing-weapon trajectory term a * b - (1 + a^2) * b^2 /
 * (2 * g_projectile_gravity_modifier) in 20.12 fixed point.
 *
 * This is the height of a ballistic arc after a horizontal distance b
 * (`progress`), where a (`launch_slope_q12`) is the tangent of the launch pitch:
 * the callers take the pitch itself as ratan2(launch_slope_q12, ONE).
 *
 * Both operands are pre-shifted by range so the squares stay within 32 bits.
 * g_projectile_gravity_modifier is the trajectory gravity modifier
 * (0xa8000).
 */
static s32 projectile_arc_calculate_trajectory_term(s32 launch_slope_q12, s32 progress) {
    s32 divisor = (g_projectile_gravity_modifier * 2) >> 12;
    s32 slope_sq;
    s32 progress_sq;
    s32 linear;

    if (launch_slope_q12 >= -0x7fff && launch_slope_q12 <= 0x7fff) {
        slope_sq = (launch_slope_q12 * launch_slope_q12) >> 12;
        if (progress >= -0xfffff && progress <= 0xfffff) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (launch_slope_q12 * (progress >> 5)) >> 7;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (launch_slope_q12 * (progress >> 7)) >> 5;
        }
    } else {
        launch_slope_q12 >>= 4;
        slope_sq = (launch_slope_q12 * launch_slope_q12) >> 4;
        if (progress >= -0xfffff && progress <= 0xfffff) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (launch_slope_q12 * (progress >> 5)) >> 3;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (launch_slope_q12 * (progress >> 7)) >> 1;
        }
    }
    return linear - (((slope_sq + ONE) >> 6) * (progress_sq >> 6)) / divisor;
}

/* Offset of a point current along a straight path of length total: dst is src,
 * the whole path's delta, scaled by current / total (a Q12 fraction). */
static void projectile_find_xyz_along_trajectory(const VECTOR* src, s32 total, s32 current, VECTOR* dst) {
    s32 progress = (current << 8) / (total >> 4);

    dst->vx = (progress * src->vx) >> 12;
    dst->vy = (progress * src->vy) >> 12;
    dst->vz = (progress * src->vz) >> 12;
}

/* Launch speed of every arcing projectile: SquareRoot12 of g_projectile_gravity_modifier times
 * g_projectile_gravity_scale. With the retail values (0xa8000 and 0x53a) it is a constant,
 * about 7.4 in Q12. */
static s32 projectile_arc_calculate_launch_velocity(void) {
    return SquareRoot12((g_projectile_gravity_modifier * g_projectile_gravity_scale) >> 12);
}

/*
 * List every present unit other than `first` and `second` that is not Jumping
 * and is not a mount carrying a rider, with its tile center and height and its
 * sprite height, into a record at the start of the scratchpad returned via `out`.
 */
static void projectile_list_obstacle_units(s32 first, s32 second, projectile_obstacle_unit_list_t** out) {
    SVECTOR position;
    unit_stats_t* unit;
    s32 i;
    s32 slot;

    *out = (projectile_obstacle_unit_list_t*)getScratchAddr(0);
    (*out)->count = 0;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = unit_get_stats(i);
        if (unit->active_battle_id != first && unit->active_battle_id != second
            && unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP))
            && !(unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT)) {
            slot = (*out)->count++;
            (*out)->battle_ids[slot] = unit->active_battle_id;
            unit_get_tile_center_and_height_by_battle_id(unit->active_battle_id, &position);
            (*out)->positions[slot] = position;
            (*out)->sprite_heights[slot] = sprite_get_unit_spritesheet_height_by_battle_id(unit->active_battle_id);
        }
    }
}

/*
 * Test a projectile position against the terrain and the listed units.
 *
 * Fills `tile_ref` with the map tile under `position` and returns 1 when the
 * position is inside that tile's solid column (either map layer) or inside a
 * listed unit's sprite box; the blocking unit's battle id is reported through
 * `out_unit`, which is otherwise left at -1.
 */
static s32 projectile_test_position_for_obstacle(const projectile_obstacle_unit_list_t* list, const VECTOR* position,
    s32* out_unit, battle_map_tile_coordinates_t* tile_ref) {
    battle_map_tile_t* tile;
    s32 ceiling;
    s32 height;
    s32 y;
    s32 i;

    *out_unit = -1;
    tile_ref->x = position->vx / BATTLE_MAP_TILE_SIZE;
    tile_ref->y = position->vz / BATTLE_MAP_TILE_SIZE;
    tile = battle_map_tile_get_pointer(tile_ref->x, tile_ref->y, 0);
    if (tile->ceiling_depth_and_marks.ceiling_depth == 0) {
        ceiling = tile->height;
    } else {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * BATTLE_MAP_HEIGHT_STEP) && y < -((height - ceiling) * BATTLE_MAP_HEIGHT_STEP)) {
        tile_ref->layer = 0;
        g_projectile_tile_flags = tile->flags._unknown_00;
        return 1;
    }

    tile = battle_map_tile_get_pointer(tile_ref->x, tile_ref->y, 1);
    ceiling = 1;
    if (tile->ceiling_depth_and_marks.ceiling_depth != 0) {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * BATTLE_MAP_HEIGHT_STEP) && y < -((height - ceiling) * BATTLE_MAP_HEIGHT_STEP)) {
        tile_ref->layer = 1;
        g_projectile_tile_flags = tile->flags._unknown_00;
        return 1;
    }

    for (i = 0; i < list->count; i++) {
        if (position->vy < list->positions[i].vy && list->positions[i].vy - list->sprite_heights[i] < position->vy
            && list->sprite_heights[i] != 0 && list->positions[i].vx - 8 < position->vx
            && position->vx < list->positions[i].vx + 8 && list->positions[i].vz - 8 < position->vz
            && position->vz < list->positions[i].vz + 8) {
            *out_unit = list->battle_ids[i];
            return 1;
        }
    }
    return 0;
}

/*
 * Step a lobbed projectile along a ballistic arc. The launch pitch comes from
 * ratan2(launch_slope_q12, ONE) and is published in both the arc descriptor and
 * g_projectile_arctan_angle_mod; the per-step advance is the horizontal
 * component of the gravity impulse. Each step rotates the local (progress,
 * -trajectory term) offset into world space through the yaw matrix and tests
 * the result against the obstacle list, exactly as
 * projectile_direct_trace_path does for straight shots. On a clear
 * path g_projectile_step_count holds the full step count and the result is 1; on a hit it
 * holds the steps taken and the result is 0.
 *
 * `hit` is deliberately left uninitialized: the target reads $s6 after a
 * zero-step loop without ever writing it.
 */
static s32 projectile_arc_trace_path(const SVECTOR* origin, const s32* launch_slope_q12, const s32* distance,
    SVECTOR* arc, const projectile_obstacle_unit_list_t* obstacles) {
    SVECTOR rotation;
    SVECTOR step;
    VECTOR position;
    VECTOR previous;
    MATRIX matrix;
    s32 angle;
    s32 velocity;
    s32 advance;
    s32 steps;
    s32 progress;
    s32 term;
    s32 hit;
    s32 i;

    angle = ratan2(*launch_slope_q12, ONE);
    arc->vz = angle;
    g_projectile_arctan_angle_mod = angle;
    progress = 0;
    velocity = projectile_arc_calculate_launch_velocity() >> 8;
    advance = (rcos(arc->vz) * velocity) >> 4;
    if (advance != 0) {
        steps = ((advance + *distance) - 1) / advance;
    } else {
        system_report_stub(1, 0);
        steps = 0;
    }

    rotation.vz = 0;
    rotation.vx = 0;
    rotation.vy = arc->vy;
    RotMatrix(&rotation, &matrix);
    position.vx = matrix.t[0] = origin->vx;
    i = 0;
    position.vy = matrix.t[1] = origin->vy;
    position.vz = matrix.t[2] = origin->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    step.vz = 0;
    for (; i < steps; i++) {
        previous = position;
        progress += advance;
        term = projectile_arc_calculate_trajectory_term(*launch_slope_q12, progress);
        step.vx = progress >> 12;
        step.vy = -term >> 12;
        RotTrans(&step, &position, &position.pad);
        hit = projectile_test_position_for_obstacle(
            obstacles, &position, &g_projectile_hit_unit_id, &g_projectile_tile);
        if (hit != 0) {
            break;
        }
    }

    setVector(&g_projectile_step, position.vx - previous.vx, position.vy - previous.vy, position.vz - previous.vz);
    copyVector(&g_projectile_position, &position);
    if (hit != 0) {
        g_projectile_step_count = i;
        return 0;
    }
    g_projectile_step_count = steps;
    return 1;
}

/* Test whether either arcing projectile path reaches the target unit.
 *
 * The arc counterpart of projectile_direct_check_between_units: the
 * source is raised by two thirds of its sprite height, projectile_arc_calculate_trajectory yields
 * the two candidate arcs for the offset, and each is traced against the
 * other units' positions. Returns 1 when an arc is clear. */
s32 projectile_arc_check_between_units(u8 source_id, u8 target_id) {
    SVECTOR rotation;
    projectile_arc_query_t query;
    projectile_obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;

    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = sprite_get_unit_spritesheet_height_by_battle_id(source_id) * 2 / 3;
    query.target.height = sprite_get_unit_spritesheet_height_by_battle_id(target_id);
    projectile_list_obstacle_units(source_id, target_id, &obstacles);
    unit_get_tile_center_and_height_by_battle_id(source_id, &query.source.position);
    query.source.position.vy -= query.source.height;
    unit_get_tile_center_and_height_by_battle_id(target_id, &query.target.position);
    query.delta.x = query.target.position.vx - query.source.position.vx;
    query.delta.z = query.target.position.vz - query.source.position.vz;
    query.delta.y = query.source.position.vy - query.target.position.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    rotation.vy = ratan2(-query.delta.z, query.delta.x);
    if (projectile_arc_calculate_trajectory(&query, &arc_a, &arc_b) != 0
        && (projectile_arc_trace_path(&query.source.position, &arc_a, &query.delta.distance, &rotation, obstacles) != 0
            || projectile_arc_trace_path(&query.source.position, &arc_b, &query.delta.distance, &rotation, obstacles)
                != 0)) {
        return 1;
    }
    return 0;
}

/*
 * Set up an arcing projectile from source_id toward target_id (or the tile at
 * tile_position when target_id is -1) and pick the arc to fly.
 *
 * Trace arc_b first, then arc_a if it is blocked; a clear arc returns target_id.
 * Otherwise an arc blocked by a unit of another team than the source wins
 * (arc_b's first) and returns
 * that unit. Failing that, arc_a's trace is kept and its blocker (or -1) is
 * returned, except when only arc_a was blocked by a unit: then arc_b's
 * terrain-blocked result is restored and -1 returned. g_projectile_launch_slope_q12 receives the
 * chosen launch slope. `unused` reserves the target's 16 otherwise unused frame bytes.
 */
s32 projectile_arc_set_and_validate(s32 source_id, const SVECTOR* tile_position, s32 target_id) {
    s32 unused[4];
    SVECTOR arc;
    VECTOR second_step;
    VECTOR first_step;
    SVECTOR second_position;
    SVECTOR first_position;
    projectile_arc_query_t query;
    projectile_obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;
    battle_map_tile_t* tile;
    unit_stats_t* stats;
    unit_stats_t* other;
    s32 hit;
    s32 second_hit;
    s32 first_unit;
    s32 first_other;
    s32 first_count;
    s32 second_unit;
    s32 second_other;
    s32 second_count;
    s32 result;
    s32 launch_slope_q12;

    g_projectile_source_id = source_id;
    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = sprite_get_unit_spritesheet_height_by_battle_id(source_id) * 2 / 3;
    if (target_id != -1) {
        query.target.height = sprite_get_unit_spritesheet_height_by_battle_id(target_id);
    } else {
        query.target.height = 0;
    }
    projectile_list_obstacle_units(source_id, -1, &obstacles);
    unit_get_tile_center_and_height_by_battle_id(source_id, &g_projectile_origin);
    g_projectile_origin.vy -= query.source.height;
    if (target_id == -1) {
        tile = battle_map_tile_get_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_projectile_destination.vx = tile_position->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vz = tile_position->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vy = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);
    } else {
        unit_get_tile_center_and_height_by_battle_id(target_id, &g_projectile_destination);
    }
    query.delta.x = g_projectile_destination.vx - g_projectile_origin.vx;
    query.delta.z = g_projectile_destination.vz - g_projectile_origin.vz;
    query.delta.y = g_projectile_origin.vy - g_projectile_destination.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    arc.vy = ratan2(-query.delta.z, query.delta.x);
    if (projectile_arc_calculate_trajectory(&query, &arc_a, &arc_b) == 0) {
        arc_b = ONE;
        arc_a = ONE;
    }

    hit = projectile_arc_trace_path(&g_projectile_origin, &arc_b, &query.delta.distance, &arc, obstacles);
    first_unit = g_projectile_hit_unit_id;
    first_step = g_projectile_step;
    first_position = g_projectile_position;
    first_count = g_projectile_step_count;
    first_other = -1;
    if (g_projectile_hit_unit_id == -1) {
        first_other = g_projectile_tile_flags;
    }
    if (hit != 0) {
        launch_slope_q12 = arc_b;
        result = target_id;
    } else {
        second_hit = projectile_arc_trace_path(&g_projectile_origin, &arc_a, &query.delta.distance, &arc, obstacles);
        second_unit = g_projectile_hit_unit_id;
        second_step = g_projectile_step;
        second_position = g_projectile_position;
        second_count = g_projectile_step_count;
        second_other = -1;
        if (g_projectile_hit_unit_id == -1) {
            second_other = g_projectile_tile_flags;
        }
        if (second_hit != 0) {
            launch_slope_q12 = arc_a;
            result = target_id;
        } else {
            stats = unit_get_stats(source_id);
            if (first_unit != -1
                && (other = unit_get_stats(first_unit),
                    (stats->team_flags & UNIT_TEAM_MASK) != (other->initial_team_flags & UNIT_TEAM_MASK))) {
                g_projectile_step = first_step;
                g_projectile_position = first_position;
                launch_slope_q12 = arc_b;
                g_projectile_hit_unit_id = first_unit;
                result = first_unit;
                g_projectile_step_count = first_count;
            } else if (second_unit != -1
                && (other = unit_get_stats(second_unit),
                    (stats->team_flags & UNIT_TEAM_MASK) != (other->initial_team_flags & UNIT_TEAM_MASK))) {
                launch_slope_q12 = arc_a;
                g_projectile_step_count = second_count;
                g_projectile_hit_unit_id = second_unit;
                result = second_unit;
            } else if (second_unit == -1) {
                launch_slope_q12 = arc_a;
                g_projectile_step_count = second_count;
                g_projectile_hit_unit_id = second_unit;
                g_projectile_tile_flags = second_other;
                result = -1;
            } else if (first_unit != -1) {
                launch_slope_q12 = arc_a;
                g_projectile_step_count = second_count;
                g_projectile_tile_flags = second_other;
                g_projectile_hit_unit_id = second_unit;
                result = second_unit;
            } else {
                g_projectile_step = first_step;
                g_projectile_position = first_position;
                launch_slope_q12 = arc_b;
                g_projectile_hit_unit_id = first_unit;
                result = -1;
                g_projectile_tile_flags = first_other;
                g_projectile_step_count = first_count;
            }
        }
    }
    g_projectile_launch_slope_q12 = launch_slope_q12;
    return result;
}

/* Choose the bow animation column from the last trajectory test's angle.
 * Wrap g_projectile_arctan_angle_mod into [-0x800, 0x800]: return column 0
 * below -0x2aa, column 1 through 0x2aa, or column 2 above it. Arc selection
 * can restore an earlier path without restoring this angle. */
s32 projectile_get_bow_animation_column(void) {
    s16 angle = g_projectile_arctan_angle_mod;
    if (angle >= 0x801) {
        g_projectile_arctan_angle_mod = angle - ONE;
    }
    {
        s16 angle_after_hi = g_projectile_arctan_angle_mod;
        if (angle_after_hi < -0x800) {
            g_projectile_arctan_angle_mod = angle_after_hi + ONE;
        }
    }
    {
        s16 wrapped = g_projectile_arctan_angle_mod;
        if (wrapped < 0x2AB) {
            return wrapped >= -0x2AA;
        }
        return 2;
    }
}

/*
 * Step a projectile from `origin` along `delta` in 0x2000 increments of the
 * `distance` total, testing each position against the obstacle list. Stores
 * the last step's movement and the final position, sets g_projectile_step_count to a
 * quarter of the steps taken, and returns 1 when the path is clear or 0 when
 * an obstacle stopped it. The `step` variable keeps the target's register
 * divide; a literal 0x2000 folds to a shift.
 */
static s32 projectile_direct_trace_path(
    const VECTOR* delta, const SVECTOR* origin, const s32* distance, const projectile_obstacle_unit_list_t* obstacles) {
    VECTOR position;
    VECTOR previous;
    s32 step;
    s32 steps;
    s32 progress;
    s32 hit;
    s32 i;

    step = 0x2000;
    steps = *distance / step;
    progress = 0;
    position.vx = origin->vx;
    hit = 0;
    position.vy = origin->vy;
    i = 0;
    position.vz = origin->vz;
    for (; i < steps; i++) {
        previous = position;
        progress += step;
        projectile_find_xyz_along_trajectory(delta, *distance, progress, &position);
        position.vx += origin->vx;
        position.vy += origin->vy;
        position.vz += origin->vz;
        hit = projectile_test_position_for_obstacle(
            obstacles, &position, &g_projectile_hit_unit_id, &g_projectile_tile);
        if (hit != 0) {
            break;
        }
    }
    setVector(&g_projectile_step, position.vx - previous.vx, position.vy - previous.vy, position.vz - previous.vz);
    copyVector(&g_projectile_position, &position);
    if (hit != 0) {
        g_projectile_step_count = i / 4;
        return 0;
    }
    g_projectile_step_count = steps / 4;
    return 1;
}

/*
 * Trace a projectile path from one unit's upper body to another's.
 *
 * Both endpoints are raised by two thirds of the unit's sprite height (12 when
 * the target has none). `unused_20` and `unused_38` reproduce unreferenced
 * frame slots the target reserves between the vectors.
 */
s32 projectile_direct_check_between_units(u8 source_id, u8 target_id) {
    SVECTOR origin;
    SVECTOR destination;
    SVECTOR unused_20;
    VECTOR delta;
    s32 unused_38[2];
    projectile_obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    source_height = sprite_get_unit_spritesheet_height_by_battle_id(source_id) * 2 / 3;
    target_height = sprite_get_unit_spritesheet_height_by_battle_id(target_id) * 2 / 3;
    if (target_height == 0) {
        target_height = 12;
    }
    projectile_list_obstacle_units(source_id, target_id, &obstacles);
    unit_get_tile_center_and_height_by_battle_id(source_id, &origin);
    origin.vy -= source_height;
    unit_get_tile_center_and_height_by_battle_id(target_id, &destination);
    destination.vy -= target_height;
    setVector(&delta, destination.vx - origin.vx, destination.vy - origin.vy, destination.vz - origin.vz);
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    return projectile_direct_trace_path(&delta, &origin, &distance, obstacles);
}

/*
 * Traces a straight projectile path from a source unit to either a target
 * unit or a bare tile, and reports whether the flight is obstructed.
 *
 * Both endpoints are raised by two thirds of their sprite height (12 when the
 * target has none), as in projectile_direct_check_between_units;
 * the tile case instead centers on the tile at tile_position (x, layer, y) and
 * sits on its surface, as in projectile_set_arrow_trajectory.  The extra
 * work over those two is the launch angle: ratan2 of the vertical delta
 * against the horizontal distance, biased by 0x400 (a quarter turn) into
 * g_projectile_arctan_angle_mod.  Returns target_id when the path is clear
 * and the blocking unit from g_projectile_hit_unit_id when it is not.
 */
s32 projectile_direct_check_to_target(s32 source_id, const SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    battle_map_tile_t* tile;
    projectile_obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    g_projectile_source_id = source_id;
    source_height = sprite_get_unit_spritesheet_height_by_battle_id(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = sprite_get_unit_spritesheet_height_by_battle_id(target_id) * 2 / 3;
        if (target_height == 0) {
            target_height = 12;
        }
    }
    projectile_list_obstacle_units(source_id, -1, &obstacles);
    unit_get_tile_center_and_height_by_battle_id(source_id, &g_projectile_origin);
    g_projectile_origin.vy -= source_height;
    if (target_id == -1) {
        tile = battle_map_tile_get_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_projectile_destination.vx = tile_position->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vz = tile_position->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
    } else {
        unit_get_tile_center_and_height_by_battle_id(target_id, &g_projectile_destination);
        g_projectile_destination.vy -= target_height;
    }
    setVector(&delta, g_projectile_destination.vx - g_projectile_origin.vx,
        g_projectile_destination.vy - g_projectile_origin.vy, g_projectile_destination.vz - g_projectile_origin.vz);
    g_projectile_arctan_angle_mod = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    if (projectile_direct_trace_path(&delta, &g_projectile_origin, &distance, obstacles) != 0) {
        return target_id;
    }
    return g_projectile_hit_unit_id;
}

/* Sets an arrow's source, destination and flight duration.
 *
 * The projectile origin is the first unit raised by two thirds of its
 * sprite height, and the destination is the second unit raised by three
 * quarters of its height, or, when target_id is -1, to the center of the tile
 * at tile_position (x, layer, y) on its surface. g_projectile_step_count receives the
 * flight duration derived from the distance, as in
 * projectile_direct_check_between_units / projectile_direct_trace_path.
 * `delta` reproduces the target's stores to an otherwise unused stack vector.
 */
void projectile_set_arrow_trajectory(s32 source_id, const SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    battle_map_tile_t* tile;
    s32 source_height = sprite_get_unit_spritesheet_height_by_battle_id(source_id) * 2 / 3;
    s32 target_height;

    if (target_id != -1) {
        target_height = sprite_get_unit_spritesheet_height_by_battle_id(target_id) * 3 / 4;
    }
    unit_get_tile_center_and_height_by_battle_id(source_id, &g_projectile_origin);
    g_projectile_origin.vy -= source_height;
    if (target_id == -1) {
        tile = battle_map_tile_get_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_projectile_destination.vx = tile_position->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vz = tile_position->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
    } else {
        unit_get_tile_center_and_height_by_battle_id(target_id, &g_projectile_destination);
        g_projectile_destination.vy -= target_height;
    }
    setVector(&delta, g_projectile_destination.vx - g_projectile_origin.vx,
        g_projectile_destination.vy - g_projectile_origin.vy, g_projectile_destination.vz - g_projectile_origin.vz);
    g_projectile_step_count
        = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE) / 8 >> 12;
}

/* Keep the secondary animation table as u8[]: another element type introduces an
 * 8-byte compiler frame and changes the load-delay and return slots. */

/* Start the selected secondary animation on the slot: reset its color and
 * script state and store the 16-bit CLUT value (callers pass CLUTs of
 * BATTLE_MAP_CLUT_ROW_WEAPONS) at 0x88. */
static void effect_slot_init_secondary_animation(s32 group, u16 clut, effect_slot_t* slot) {
    u32 offset;

    slot->clut = clut;
    offset = *(u16*)(g_effect_secondary_particle_animation_data
        + (g_effect_secondary_particle_configs[group].animation_id << 1));
    slot->color.b = 0x80;
    slot->color.g = 0x80;
    slot->color.r = 0x80;
    slot->step_index = 0;
    slot->hold = 0;
    slot->angle = 0;
    slot->script = (const effect_anim_script_t*)(g_effect_secondary_particle_animation_data + offset);
}

/* Spawn this frame's share of a particle group's effect slots, drawn with CLUT
 * clut_index of the weapons row.
 *
 * The group spawns while the secondary effect's timer is inside its spawn
 * frames. Its spawns_per_frame caps how many slots one frame may start; the
 * secondary effect's active_count counts the live slots and stops the
 * spawning at the group's total count. */
static void effect_secondary_start_group(s32 group, s32 clut_index) {
    effect_slot_t* slot;
    s32 spawned;
    s32 i;
    u8 id;

    if (g_effect_secondary_particle_configs[group].count > g_effect_secondary_current->active_count
        && g_effect_secondary_current->timer >= g_effect_secondary_particle_configs[group].spawn_start_frame
        && g_effect_secondary_current->timer < g_effect_secondary_particle_configs[group].spawn_end_frame) {
        for (spawned = 0; spawned < g_effect_secondary_particle_configs[group].spawns_per_frame; spawned++) {
            for (i = 0; i < g_effect_secondary_particle_configs[group].count; i++) {
                if (g_effect_secondary_current->slot_ids[i] == 0) {
                    break;
                }
            }
            id = effect_slot_alloc();
            g_effect_secondary_current->slot_ids[i] = id;
            if (id == 0) {
                break;
            }
            slot = &g_effect_slots[g_effect_secondary_current->slot_ids[i]];
            slot->life = effect_slot_spawn_motion(&g_effect_secondary_particle_configs[group], &slot->motion);
            effect_slot_init_secondary_animation(
                group, clut_index + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 0), slot);
            g_effect_secondary_current->active_count++;
            if (g_effect_secondary_current->active_count == g_effect_secondary_particle_configs[group].count) {
                break;
            }
        }
    }
}

/* Advance the current secondary effect's particles: every effect slot in the
 * first count entries of slot_ids, count being the particle group's. A slot that
 * has finished is freed and active_count drops by one. */
static void effect_secondary_update_active_slots(s32 group) {
    s32 i;
    s32 id;

    for (i = 0; i < g_effect_secondary_particle_configs[group].count; i++) {
        id = g_effect_secondary_current->slot_ids[i];
        if (id != 0 && effect_slot_update(id) == 0) {
            effect_slot_free(id);
            g_effect_secondary_current->slot_ids[i] = 0;
            g_effect_secondary_current->active_count--;
        }
    }
}

/* Secondary effect function 0x00 in g_effect_secondary_handlers: returns 0, so a record
 * with no function is removed on its first update. */
s32 effect_secondary_none_handler(void) {
    return 0;
}

/* An empty routine among the secondary-effect handlers. Nothing references it, not
 * even g_effect_secondary_handlers. */
void effect_noop_4(void) {
}

/*
 * Secondary-effect handler for bow arrows; crossbow bolts use the straight-flight handler.
 *
 * Phase 1 derives the arrow's heading, launch pitch and per-step gravity from
 * the trajectory origin/destination, builds its rotation matrix and allocates
 * the polygon work buffer. Phase 2 advances one step along the arc, orients
 * the arrow along the local slope and draws it with effect_model_draw; the last
 * step or a positive height ends the flight. Phase 3 returns 0. An unknown
 * phase returns `result` uninitialized, as the target returns $s6 unwritten.
 */
s32 effect_secondary_arrow_arc_handler(void) {
    SVECTOR rotation;
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    effect_arrow_buffer_t* work;
    s32 velocity;
    s32 term;
    s32 previous;
    s32 next;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;
    u8 flags;

    switch (g_effect_secondary_current->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        delta.vx = g_projectile_destination.vx - g_projectile_origin.vx;
        delta.vz = g_projectile_destination.vz - g_projectile_origin.vz;
        delta.vy = g_projectile_origin.vy - g_projectile_destination.vy;
        g_effect_arrow_arc_length = SquareRoot12((delta.vx * delta.vx + delta.vz * delta.vz) * ONE);
        g_effect_arrow_arc_yaw = ratan2(-delta.vz, delta.vx);
        result = 1;
        g_effect_arrow_arc_caster_relative_yaw = g_effect_arrow_arc_yaw
            - (((unit_facing_get_quadrant_by_runtime_id(g_effect_secondary_current->caster.data.unit.runtime_id) + 1)
                   & 3)
                << 10);
        g_effect_arrow_launch_slope_q12 = g_projectile_launch_slope_q12;
        g_effect_arrow_arc_launch_angle = ratan2(g_projectile_launch_slope_q12, ONE);
        g_effect_arrow_arc_distance = 0;
        velocity = projectile_arc_calculate_launch_velocity() >> 8;
        g_effect_arrow_arc_distance_step = (rcos(g_effect_arrow_arc_launch_angle) * velocity) >> 4;
        g_effect_projectile_step_count = g_projectile_step_count;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = g_effect_arrow_arc_yaw;
        RotMatrix(&rotation, &g_effect_projectile_matrix);
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        if (g_effect_secondary_current->allocation != 0) {
            heap_free(g_effect_secondary_current->allocation);
        }
        work = heap_alloc(0xc88);
        g_effect_secondary_current->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 4; j++) {
                SetPolyG3(&work->g3[i][j]);
            }
            for (j = 0; j < 13; j++) {
                SetPolyG4(&work->g4[i][j]);
            }
        }
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        previous
            = -projectile_arc_calculate_trajectory_term(g_effect_arrow_launch_slope_q12, g_effect_arrow_arc_distance);
        g_effect_arrow_arc_distance += g_effect_arrow_arc_distance_step;
        term = -projectile_arc_calculate_trajectory_term(g_effect_arrow_launch_slope_q12, g_effect_arrow_arc_distance);
        next = -projectile_arc_calculate_trajectory_term(
            g_effect_arrow_launch_slope_q12, g_effect_arrow_arc_distance + g_effect_arrow_arc_distance_step);
        setVector(
            &angles, 0, g_effect_arrow_arc_yaw, ratan2(next - previous, g_effect_arrow_arc_distance_step * 2) + 0x400);
        SetRotMatrix(&g_effect_projectile_matrix);
        SetTransMatrix(&g_effect_projectile_matrix);
        rotation.vy = term >> 12;
        rotation.vz = 0;
        rotation.vx = g_effect_arrow_arc_distance >> 12;
        RotTrans(&rotation, &delta, &delta.pad);
        setVector(&position, g_projectile_origin.vx + delta.vx, g_projectile_origin.vy + delta.vy,
            g_projectile_origin.vz + delta.vz);
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        setVector(&rotation, g_projectile_origin.vx + delta.vx, g_projectile_origin.vy + delta.vy,
            g_projectile_origin.vz + delta.vz);
        RotTrans(&rotation, &delta, &delta.pad);
        scale.vz = 0x40;
        scale.vy = 0x40;
        scale.vx = 0x40;
        parameter = g_effect_secondary_animations[g_effect_secondary_current->animation_id].parameter;
        spin.vz = 0;
        spin.vy = 0;
        spin.vx = 0;
        effect_model_draw(&spin, &position, &angles, &scale, parameter);
        if (g_effect_projectile_step_count == 2 && g_projectile_hit_unit_id != -1
            && g_effect_secondary_current->target_count != 0) {
            action_start_projectile_hit_response_by_runtime_id(g_effect_secondary_current->caster.data.unit.runtime_id,
                g_effect_secondary_current->target.record.data.unit.runtime_id);
        }
        if (--g_effect_projectile_step_count == 0 || position.vy >= 0) {
            g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
            flags = g_effect_secondary_current->target.record.data.unit.result_animation;
            if (flags == EFFECT_HIT_MISSED || flags - 4 < 2U || g_projectile_hit_unit_id == -1) {
                g_effect_arrow_miss_marker = 0x80000000;
                effect_secondary_end_on_current_unit();
            }
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/* 0x28-byte heap state kept in the secondary effect's allocation. */
typedef struct effect_burst_state {
    s32 frame;    /* 0x00 */
    s32 done;     /* 0x04 */
    s32 count_a;  /* 0x08 */
    u8 ids_a[8];  /* 0x0c; group 0 slots */
    s32 count_b;  /* 0x14 */
    u8 ids_b[16]; /* 0x18; group 1 or 9 slots */
} effect_burst_state_t;

/* Secondary effect function 0x02 in g_effect_secondary_handlers: the burst of a weapon
 * hit (EFFECT_SECONDARY_ANIMATION_WEAPON_HIT). */
s32 effect_secondary_weapon_hit_handler(void) {
    effect_secondary_data_t* effect;
    effect_burst_state_t* state;
    effect_slot_t* slot;
    s32 unused[8];
    vec3s32_t gravity;
    VECTOR zoom;
    VECTOR offset;
    s32 result;
    s32 group;
    s32 j;
    s32 done;
    s32 i;
    u8 id;

    effect = g_effect_secondary_current;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        if (effect->target.record.data.unit.result_animation == EFFECT_HIT_NO_EFFECT) {
            return 0;
        }
        if (effect->allocation != 0) {
            heap_free(effect->allocation);
        }

        state = heap_alloc(sizeof(effect_burst_state_t));
        g_effect_secondary_current->allocation = state;
        state->frame = 0;
        state->done = 0;
        state->count_a = 0;
        state->count_b = 0;
        for (i = 0; i < 8; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 16; i++) {
            state->ids_b[i] = 0;
        }

        result = 1;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        effect_camera_seed_tracks();
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        state = effect->allocation;
        group = EFFECT_PARTICLE_GROUP_WEAPON_HIT_B;
        if (effect->target.record.data.unit.result_animation == EFFECT_HIT_MISSED) {
            group = EFFECT_PARTICLE_GROUP_WEAPON_HIT_B_MISSED;
        }

        /* A landed hit flashes the target. group is 1 here, so the frame == group test restores
         * the palette one frame after the flash. */
        if (group == EFFECT_PARTICLE_GROUP_WEAPON_HIT_B) {
            if (state->frame == 0) {
                sprite_palette_apply_modulation_to_units(
                    BATTLE_MAP_COLOR_MODE_BASE_HALF, 0, effect->target.record.data.unit.runtime_id, 0x1f, 0x1f, 0x1f);
            }
            if (state->frame == group) {
                sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_RESTORE, 2,
                    g_effect_secondary_current->target.record.data.unit.runtime_id, 0, 0, 0);
            }
        }

        /* Spawn both particle sets over the first four frames: set A with a CLUT offset by the
         * effect parameter, set B with a fixed one. The counts below 8 and 16 guarantee the
         * free-entry searches succeed. */
        if (state->frame < 4) {
            if (state->count_a < 8) {
                for (i = 0;
                    i < g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A].spawns_per_frame; i++) {
                    for (j = 0; j < 8; j++) {
                        if (state->ids_a[j] == 0) {
                            break;
                        }
                    }
                    state->ids_a[j] = effect_slot_alloc();
                    if (state->ids_a[j] == 0) {
                        break;
                    }
                    slot = &g_effect_slots[state->ids_a[j]];
                    slot->life = effect_slot_spawn_motion(
                        &g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A], &slot->motion);
                    effect_slot_init_secondary_animation(EFFECT_PARTICLE_GROUP_WEAPON_HIT_A,
                        g_effect_secondary_current->parameter + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 0), slot);
                    state->count_a++;
                    if (state->count_a == 8) {
                        break;
                    }
                }
            }

            if (state->frame < 4 && state->count_b < 16) {
                for (i = 0; i < g_effect_secondary_particle_configs[group].spawns_per_frame; i++) {
                    for (j = 0; j < 16; j++) {
                        if (state->ids_b[j] == 0) {
                            break;
                        }
                    }
                    state->ids_b[j] = effect_slot_alloc();
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                    slot = &g_effect_slots[state->ids_b[j]];
                    slot->life = effect_slot_spawn_motion(&g_effect_secondary_particle_configs[group], &slot->motion);
                    effect_slot_init_secondary_animation(
                        EFFECT_PARTICLE_GROUP_WEAPON_HIT_B, BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 10), slot);
                    slot->color.r = slot->color.g = slot->color.b = 0xff;
                    state->count_b++;
                    if (state->count_b == 16) {
                        break;
                    }
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        /* Step the live particles under the secondary-effect inertia and gravity, then put back
         * the effect file's own values. */
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);

        for (i = 0; i < 8; i++) {
            id = state->ids_a[i];
            if (id != 0 && effect_slot_update(id) == 0) {
                effect_slot_free(id);
                state->ids_a[i] = 0;
                state->count_a--;
            }
        }
        for (i = 0; i < 16; i++) {
            id = state->ids_b[i];
            if (id != 0 && effect_slot_update(id) == 0) {
                effect_slot_free(id);
                state->ids_b[i] = 0;
                state->count_b--;
            }
        }

        effect_restore_inertia_threshold();
        effect_restore_gravity();

        /* A critical hit pulses the zoom around the saved value over frames 0-4, then shakes the camera. */
        if (g_effect_secondary_current->target.record.data.unit.result_animation == EFFECT_HIT_CRITICAL) {
            if (state->frame == 0) {
                zoom.vx = zoom.vy = zoom.vz = -0x80;
                effect_camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 2) {
                zoom.vx = zoom.vy = zoom.vz = 0x2e4;
                effect_camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 4) {
                zoom.vx = zoom.vy = zoom.vz = 0;
                effect_camera_execute_zoom_command(
                    EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 12, &zoom);
                offset.vx = offset.vy = offset.vz = 4;
                effect_camera_execute_position_command(
                    EFFECT_CAMERA_SOURCE_GIVEN | EFFECT_CAMERA_MODE_SHAKE_DECAYING, 0, 9, &offset);
            }
        }

        /* done counts up once nothing is live after frame 31; the handler returns 0 only on
         * the frame after it first counts. */
        done = state->done;
        result = done != 1;
        state->frame++;
        if (state->count_a == 0 && state->count_b == 0 && state->frame >= 31) {
            state->done = done + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x03 in g_effect_secondary_handlers: the
 * particles of EFFECT_SECONDARY_ANIMATION_GOLEM.
 *
 * Initializing clears the counters. Executing starts this frame's share of the
 * Golem particle group with CLUT 13 of the weapons row and, like Finalizing,
 * steps the live slots under the secondary-effect inertia threshold and a gravity
 * of (0, ONE, 0), then puts back the effect file's own values. Once no slot is
 * live and the timer has passed the group's spawn_start_frame, finish_timer
 * counts up; the handler returns 0 on the frame after that begins, and 1 until
 * then. */
s32 effect_secondary_golem_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6];
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_secondary_current;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_secondary_start_group(EFFECT_PARTICLE_GROUP_GOLEM, 13);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(EFFECT_PARTICLE_GROUP_GOLEM);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_secondary_current;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_GOLEM].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* The target compares a line's two endpoints as 32-bit values. */
#define LINE_G2_XY_WORD(line, n) (*(s32*)&line->x##n)

/* Secondary handler for a charge effect: phase 1 loads the animation's
 * parameters and primes the line primitives, phase 2 spawns trails that
 * converge on the caster, and phases 2-3 advance and draw them.
 *
 * The OT link masks `line` before `setaddr(line, getaddr(ot + 1))`: that keeps
 * the 0xffffff mask's loop lifetime short enough for loop.c to leave it in the
 * inner loop, and getaddr's extra mask reference gives it priority over the
 * 0xff000000 mask in local allocation (a1 versus a2 in the target). */
s32 effect_secondary_spell_charge_handler(void) {
    effect_secondary_data_t* effect;
    effect_charge_state_t* state;
    SVECTOR in;
    s32 unused[4];
    VECTOR out;
    LINE_G2* line;
    SVECTOR* screen;
    effect_slot_t* slot;
    u32* ot;
    s16* offsets_x;
    s16* offsets_y;
    s32 result;
    s32 index;
    s32 i;
    s32 j;
    s32 k;
    s32 age;
    s32 head;
    s32 fade;
    s32 x;
    s32 y;
    s32 dir;
    s32 over;
    u32 red;
    u32 green;
    u32 blue;
    u32 link;
    effect_slot_t* work;

    effect = g_effect_secondary_current;
    state = &g_effect_secondary_state.charge;
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        index = g_effect_secondary_animations[effect->animation_id].parameter;
        /* The animation parameter picks the line parameters; the effect parameter is replaced by
         * its element index, which picks the trail color. */
        g_effect_secondary_current->parameter = effect_get_element_index(effect->parameter);
        g_effect_secondary_state.charge.speed = g_effect_charge_line_params[index].speed;
        g_effect_secondary_state.charge.spread_range = g_effect_charge_line_params[index].spread_range;
        g_effect_secondary_state.charge.spread_offset = g_effect_charge_line_params[index].spread_offset;
        g_effect_secondary_state.charge.duration = g_effect_charge_line_params[index].duration;
        g_effect_secondary_state.charge.radius = g_effect_charge_line_params[index].radius;
        g_effect_secondary_state.charge.spawn_interval = g_effect_charge_line_params[index].spawn_interval;
        g_effect_secondary_state.charge.color = g_effect_secondary_current->parameter;
        g_effect_secondary_state.charge.max_trails = g_effect_charge_line_params[index].max_trails;
        g_effect_secondary_state.charge.fade_row = g_effect_charge_line_params[index].fade_row;

        /* Two buffers of 96 additive Gouraud lines: 16 trails of 6 segments each. */
        SetDrawMode(&state->mode[0], 1, 1, PSYQ_GPU_TPAGE_ABR_ADD, 0);
        SetDrawMode(&state->mode[1], 1, 1, PSYQ_GPU_TPAGE_ABR_ADD, 0);
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 96; j++) {
                SetLineG2(&state->lines[i][j]);
                SetSemiTrans(&state->lines[i][j], 1);
            }
        }

        for (i = 0; i < 16; i++) {
            state->trails[i].slot_id = 0;
        }
        state->count.trails = 0;
        state->count.auxiliary_particles = 0;
        state->count.particles = 0;
        for (i = 0; i < 2; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 14; i++) {
            state->ids_b[i] = 0;
        }

        /* Spawn the particles 8 units above the caster's sprite, and project that point to the
         * screen as the center the trails converge on. */
        screen = unit_get_world_coords_by_runtime_id(g_effect_secondary_current->caster.data.unit.runtime_id);
        i = sprite_get_unit_spritesheet_height_by_runtime_id(g_effect_secondary_current->caster.data.unit.runtime_id)
            + 8;
        g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].position.y
            = g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE_13].position.y = -i;
        in.vx = screen->vx;
        in.vz = screen->vz;
        in.vy = screen->vy - i;
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&in, &out, &out.pad);
        state->angle = 0;
        state->done = 0;
        state->x = out.vx;
        state->y = out.vy;

        suzuki_sound_play_sfx(SOUND_SFX_SPELL_CHARGE);
        result = 1;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        /* With a 1 in spawn_interval chance per frame, start a trail on the circle of radius
         * around the center. */
        if (g_effect_secondary_state.charge.done == 0
            && g_effect_secondary_state.charge.count.trails < g_effect_secondary_state.charge.max_trails
            && rand() % g_effect_secondary_state.charge.spawn_interval == 0) {
            for (i = 0; i < 16; i++) {
                if (state->trails[i].slot_id == 0) {
                    j = state->trails[i].slot_id = effect_slot_alloc();
                    if (j == 0) {
                        break;
                    }
                    state->count.trails++;
                    k = (rand() & 0x1ff) + state->angle;
                    /* 0x571 is about a third of a turn, so successive trails spread around the circle. */
                    state->angle += 0x571;
                    g_effect_slots[j].motion.position.x = rcos(k) * state->radius + (state->x * ONE);
                    g_effect_slots[j].motion.position.y = rsin(k) * state->radius + (state->y * ONE);
                    /* Half a turn (0x800) aims the velocity back toward the center, give or take the spread. */
                    k = k + rand() % state->spread_range - state->spread_offset - 0x800;
                    g_effect_slots[j].motion.velocity.x = rcos(k) * state->speed;
                    g_effect_slots[j].motion.velocity.y = rsin(k) * state->speed;
                    g_effect_slots[j].life = 1;
                    /* Seed all seven history points with the start so the new trail has zero length. */
                    for (k = 0; k < 7; k++) {
                        state->trails[i].points[k].vx = g_effect_slots[j].motion.position.x >> 12;
                        state->trails[i].points[k].vy = g_effect_slots[j].motion.position.y >> 12;
                    }
                    state->trails[i].age = 0;
                    break;
                }
            }
        }

        /* Top up the spell-charge particle group, at most 14 live at once. */
        if (state->count.particles < 14) {
            for (i = 0; i < g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].spawns_per_frame;
                i++) {
                for (j = 0; j < 14; j++) {
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                }
                state->ids_b[j] = effect_slot_alloc();
                if (state->ids_b[j] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids_b[j]];
                slot->life = effect_slot_spawn_motion(
                    &g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE], &slot->motion);
                effect_slot_init_secondary_animation(
                    EFFECT_PARTICLE_GROUP_SPELL_CHARGE, BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 15), slot);
                state->count.particles++;
                if (state->count.particles == 14) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        /* Unlike the other secondary handlers, this one never restores the saved inertia threshold. */
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);

        for (i = 0; i < 2; i++) {
            j = state->ids_a[i];
            if (j != 0 && effect_slot_update(j) == 0) {
                effect_slot_free(j);
                state->ids_a[i] = 0;
                state->count.auxiliary_particles--;
            }
        }
        for (i = 0; i < 14; i++) {
            j = state->ids_b[i];
            if (j != 0 && effect_slot_update(j) == 0) {
                effect_slot_free(j);
                state->ids_b[i] = 0;
                state->count.particles--;
            }
        }

        /* Trails: move each head, ease its newest point toward the center, then link its six
         * segments from newest to oldest point, dimming toward the tail. */
        red = g_effect_charge_trail_colors[state->color].r;
        green = g_effect_charge_trail_colors[state->color].g;
        blue = g_effect_charge_trail_colors[state->color].b;
        offsets_x = &g_effect_charge_line_direction_offsets[0];
        offsets_y = &g_effect_charge_line_direction_offsets[1];
        for (i = 0; i < 16; i++) {
            j = state->trails[i].slot_id;
            if (j != 0) {
                work = &g_effect_slots[j];
                work->motion.position.x = x = g_effect_slots[j].motion.position.x + g_effect_slots[j].motion.velocity.x;
                work->motion.position.y = y = g_effect_slots[j].motion.position.y + g_effect_slots[j].motion.velocity.y;
                /* points[] is a seven-entry ring; age % 7 is the newest entry. */
                age = ++state->trails[i].age;
                head = age % 7;
                if (age <= state->duration) {
                    state->trails[i].points[head].vx
                        = effect_interpolate_s32_cos(x >> 12, state->x, state->duration, age);
                    fade = 6;
                    state->trails[i].points[head].vy
                        = effect_interpolate_s32_cos(y >> 12, state->y, state->duration, age);
                } else {
                    /* Past the duration the head sits on the center and the whole trail fades out. */
                    state->trails[i].points[head].vx = state->x;
                    state->trails[i].points[head].vy = state->y;
                    over = age - 6;
                    fade = state->duration - over;
                    if (fade < 0) {
                        fade = 0;
                    }
                }

                for (k = 0; k < 6; k++) {
                    line = &state->lines[g_effect_buffer_index][i * 6 + k];
                    line->x0 = state->trails[i].points[head].vx;
                    line->y0 = state->trails[i].points[head].vy;
                    setRGB0(line,
                        red * g_effect_charge_line_fade_levels[state->fade_row * EFFECT_CHARGE_LINE_FADE_STEPS + fade]
                            >> 8,
                        green * g_effect_charge_line_fade_levels[state->fade_row * EFFECT_CHARGE_LINE_FADE_STEPS + fade]
                            >> 8,
                        blue * g_effect_charge_line_fade_levels[state->fade_row * EFFECT_CHARGE_LINE_FADE_STEPS + fade]
                            >> 8);
                    if (--head < 0) {
                        head = 6;
                    }
                    age--;
                    if (fade != 0) {
                        fade--;
                    }
                    /* Points from before the trail was born draw black, which the additive mode hides. */
                    if (age < 0) {
                        fade = 0;
                    }
                    line->x1 = state->trails[i].points[head].vx;
                    line->y1 = state->trails[i].points[head].vy;
                    setRGB1(line,
                        red * g_effect_charge_line_fade_levels[state->fade_row * EFFECT_CHARGE_LINE_FADE_STEPS + fade]
                            >> 8,
                        green * g_effect_charge_line_fade_levels[state->fade_row * EFFECT_CHARGE_LINE_FADE_STEPS + fade]
                            >> 8,
                        blue * g_effect_charge_line_fade_levels[state->fade_row * EFFECT_CHARGE_LINE_FADE_STEPS + fade]
                            >> 8);
                    /* Link only segments with length. The +0x900 shifts ratan2's -0x800..0x800 into the
                     * positive range before taking one of the 512-unit direction sectors. */
                    if (LINE_G2_XY_WORD(line, 0) != LINE_G2_XY_WORD(line, 1)) {
                        dir = (ratan2(line->y1 - line->y0, line->x1 - line->x0) + 0x900) / 512;
                        line->x1 += offsets_x[dir * 2];
                        line->y1 += offsets_y[dir * 2];
                        ot = g_effect_polygon_depth_data;
                        link = (u32)line & 0xffffff;
                        setaddr(line, getaddr(ot + 1));
                        ot[1] = (ot[1] & 0xff000000) | link;
                    }
                }

                /* Free the trail once all seven points have settled on the center. */
                if (state->trails[i].age >= state->duration + 7) {
                    effect_slot_free(j);
                    state->trails[i].slot_id = 0;
                    state->count.trails--;
                }
            }
        }

        /* Added last at the same depth, so the draw mode runs before the lines. */
        AddPrim(g_effect_polygon_depth_data + 1, &state->mode[g_effect_buffer_index]);

        /* Keep running while executing or while anything is still live; then stop the charge sound. */
        if (g_effect_secondary_current->phase == EFFECT_SECONDARY_PHASE_EXECUTING
            || (state->count.trails != 0 || state->count.auxiliary_particles != 0 || state->count.particles != 0)) {
            result = 1;
        } else {
            suzuki_sound_stop_sfx_channels(SOUND_SFX_SPELL_CHARGE);
            result = 0;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x05: an erroneous
 * secondary animation that only steps its state so that a null animation can
 * finish cleanly.  Returns 1 to keep animating, 0 to stop.
 */
s32 effect_secondary_null_handler(void) {
    s32 result;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_secondary_current->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x06 (Red Orbs) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 8, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 effect_secondary_red_orbs_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_secondary_current;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(SOUND_SFX_RED_ORBS);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_secondary_start_group(EFFECT_PARTICLE_GROUP_RED_ORBS, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(EFFECT_PARTICLE_GROUP_RED_ORBS);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_RED_ORBS].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x07 in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_secondary_update
 * tests one to decide whether the effect ends. */
s32 effect_secondary_unused_07_handler(void) {
}

/*
 * Secondary effect function 0x08 (the dust of a fall) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 effect_secondary_fall_dust_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_secondary_current->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_secondary_current->active_count = 0;
        g_effect_secondary_current->timer = 0;
        g_effect_secondary_current->finish_timer = 0;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_secondary_start_group(EFFECT_PARTICLE_GROUP_FALL_DUST, 9);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(EFFECT_PARTICLE_GROUP_FALL_DUST);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_FALL_DUST].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x09 (Splash) in g_effect_secondary_handlers.
 * It shares the fall dust
 * handler's shape; each executing frame first lifts group 7 to the water
 * surface of the target's tile, 12 units per height and depth step above the
 * target's screen z. Returns 1 to keep animating.
 */
s32 effect_secondary_splash_handler(void) {
    effect_secondary_data_t* effect;
    battle_map_tile_t* tile;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_secondary_current->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_secondary_current->active_count = 0;
        g_effect_secondary_current->timer = 0;
        g_effect_secondary_current->finish_timer = 0;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        tile = battle_map_get_unit_tile_from_world_coords(
            g_effect_secondary_current->target.record.data.unit.runtime_id);
        g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].position.y
            = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP)
            - unit_get_world_coords_by_runtime_id(g_effect_secondary_current->target.record.data.unit.runtime_id)->vy;
        effect_secondary_start_group(EFFECT_PARTICLE_GROUP_SPLASH, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(EFFECT_PARTICLE_GROUP_SPLASH);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0a in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_secondary_update
 * tests one to decide whether the effect ends. */
s32 effect_secondary_unused_0a_handler(void) {
}

/* Secondary effect function 0x0b in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_secondary_update
 * tests one to decide whether the effect ends. */
s32 effect_secondary_unused_0b_handler(void) {
}

/*
 * Secondary effect function 0x0c (Trap) in g_effect_secondary_handlers, for every trap kind.
 * It shares the golem handler's
 * shape: initialize and play sound 0x22, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 effect_secondary_trap_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_secondary_current;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(SOUND_SFX_TRAP);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_secondary_start_group(EFFECT_PARTICLE_GROUP_TRAP, 12);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(EFFECT_PARTICLE_GROUP_TRAP);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_TRAP].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x0d (Level Up) in g_effect_secondary_handlers.
 * It shares the trap
 * handler's shape but seeds group 5's position_y with -8 and lowers it by 3 per
 * executing frame instead of playing a sound. Returns 1 to keep animating.
 */
s32 effect_secondary_level_up_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_secondary_current;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y = -8;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_secondary_start_group(EFFECT_PARTICLE_GROUP_LEVEL_UP, 11);
        {
            /* Addressing through a pointer keeps the target lui/addiu pair. */
            s16* rise = &g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y;
            *rise -= 3;
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(EFFECT_PARTICLE_GROUP_LEVEL_UP);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_secondary_current;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0e in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_secondary_update
 * tests one to decide whether the effect ends. */
s32 effect_secondary_unused_0e_handler(void) {
}

enum { EFFECT_DEATH_POOF_SLOT_COUNT = 2, EFFECT_DEATH_POOF_CLUT = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 15) };

/*
 * Death Poof secondary effect (0x0f) handler for the two-slot particle group 4: it
 * starts this frame's share of the group's effect slots, steps them with a
 * gravity of (0, ONE, 0) and finishes once the spawn state's finish_timer
 * reaches 1.
 *
 * A phase outside the three supported states leaves the result indeterminate,
 * preserving the original register residue.
 */
s32 effect_secondary_death_poof_handler(void) {
    effect_secondary_data_t* effect;
    effect_death_poof_state_t* state;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s32 spawned;
    s32 i;

    effect = g_effect_secondary_current;
    state = &g_effect_death_poof_states[effect->own_slot_id];
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING: {
        state->count = 0;
        for (i = 0; i < EFFECT_DEATH_POOF_SLOT_COUNT; i++) {
            state->ids[i] = 0;
        }
        state->finish_timer = 0;
        g_effect_secondary_current->timer = 0;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    }
    case EFFECT_SECONDARY_PHASE_EXECUTING: {
        effect_slot_t* slot;
        s32 i;
        u8 id;

        if (effect->timer >= g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame
            && effect->timer < g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_DEATH_POOF_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = effect_slot_alloc();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids[i]];
                slot->life = effect_slot_spawn_motion(
                    &g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF], &slot->motion);
                effect_slot_init_secondary_animation(EFFECT_PARTICLE_GROUP_DEATH_POOF, EFFECT_DEATH_POOF_CLUT, slot);
                state->count++;
                if (state->count == EFFECT_DEATH_POOF_SLOT_COUNT) {
                    break;
                }
            }
        }
    }
    /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING: {
        effect_secondary_data_t* current;
        u8 id;

        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        for (i = 0; i < EFFECT_DEATH_POOF_SLOT_COUNT; i++) {
            id = state->ids[i];
            if (id != 0 && effect_slot_update(id) == 0) {
                effect_slot_free(id);
                state->ids[i] = 0;
                state->count--;
            }
        }
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_secondary_current;
        current->timer = current->timer + 1;
        result = state->finish_timer != 1;
        if (state->count == 0
            && current->timer
                > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame) {
            state->finish_timer = state->finish_timer + 1;
        }
        break;
    }
    }
    return result;
}

/* Secondary effect function 0x10 in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_secondary_update
 * tests one to decide whether the effect ends. */
s32 effect_secondary_unused_10_handler(void) {
}

enum { EFFECT_ZODIAC_POOF_SLOT_COUNT = 24 };

/* Secondary effect function 0x11 (Zodiac Poof): the 24-slot particle group 3.
 *
 * Its spawn state is fixed at 0x801bade0 and its CLUT comes from the table
 * indexed by the effect's parameter, the caster's sprite palette. It follows
 * the same phase structure as the death-poof secondary handler.
 */
s32 effect_secondary_zodiac_poof_handler(void) {
    effect_secondary_data_t* effect;
    effect_zodiac_poof_state_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s32 spawned;
    s32 i;
    s32 done;
    s16 timer;
    u8 id;
    u16 clut_index;

    effect = g_effect_secondary_current;
    state = &g_effect_secondary_state.zodiac_poof;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->count = 0;
        for (spawned = 0; spawned < EFFECT_ZODIAC_POOF_SLOT_COUNT; spawned++) {
            state->ids[spawned] = 0;
        }
        state->done = 0;
        g_effect_secondary_current->timer = 0;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        clut_index = g_effect_zodiac_poof_animations[effect->parameter];
        if (effect->timer >= g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame
            && effect->timer < g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_ZODIAC_POOF_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = effect_slot_alloc();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids[i]];
                slot->life = effect_slot_spawn_motion(
                    &g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF], &slot->motion);
                effect_slot_init_secondary_animation(EFFECT_PARTICLE_GROUP_ZODIAC_POOF,
                    clut_index + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 0), slot);
                state->count++;
                if (state->count == EFFECT_ZODIAC_POOF_SLOT_COUNT) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        for (spawned = 0; spawned < EFFECT_ZODIAC_POOF_SLOT_COUNT; spawned++) {
            id = state->ids[spawned];
            if (id != 0 && effect_slot_update(id) == 0) {
                effect_slot_free(id);
                state->ids[spawned] = 0;
                state->count--;
            }
        }
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        {
            effect_secondary_data_t* current_effect;

            current_effect = g_effect_secondary_current;
            timer = current_effect->timer + 1;
            current_effect->timer = timer;
        }
        done = state->done;
        result = done != 1;
        if (state->count == 0
            && timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame) {
            state->done = done + 1;
        }
        break;
    }
    return result;
}

/* Secondary-effect handler 0x12 (Reflect). The effect's parameter is the
 * frames the shield stays, at least 26: the child timeline passes the
 * effect's spawn_delay.
 *
 * Phase 1 allocates and initializes the work buffer; phase 2 advances a
 * two-entry CLUT wipe near the end of the effect, uploads the CLUT, aims the
 * shield from the target's center toward the caster and hands the transform
 * to effect_model_draw; phase 3 finishes. The wipe indices are assigned inside the
 * subscript, each to its own variable: a separate statement or a folded
 * `timer - 1 - span` changes the order and association of the index
 * arithmetic. spin_timer keeps the timer load at full width (lh). */
s32 effect_secondary_reflect_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    RECT rect;
    effect_reflect_buffer_t* work;
    SVECTOR* caster;
    SVECTOR* target;
    rgb24_t* colors;
    u8* blue;
    u8* green;
    u8* red;
    s32 span;
    s32 timer;
    s32 lit;
    s32 cleared;
    s32 spin_timer;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;

    span = g_effect_secondary_current->parameter;
    if (span < 0x1a) {
        span = 0x1a;
    }
    switch (g_effect_secondary_current->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        if (g_effect_secondary_current->allocation != 0) {
            heap_free(g_effect_secondary_current->allocation);
        }
        work = heap_alloc(0x798);
        g_effect_secondary_current->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 24; j++) {
                SetPolyGT3(&work->prims[i][j]);
                SetSemiTrans(&work->prims[i][j], 1);
            }
        }
        /* Separate channel pointers retain the target's three stride bases. */
        for (i = 0; i < 2; i++) {
            colors = g_effect_reflect_palette_colors;
            red = &colors->r;
            green = &colors->g;
            blue = &colors->b;
            for (j = 0; j < 6; j++) {
                work->clut[i][j] = red[j * sizeof(*colors)] | (green[j * sizeof(*colors)] << 5)
                    | (blue[j * sizeof(*colors)] << 10) | 0x8000;
            }
        }
        result = 1;
        g_effect_secondary_current->timer = 0;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        work = g_effect_secondary_current->allocation;
        timer = g_effect_secondary_current->timer;
        if (span < timer && timer < span + 4) {
            work->clut[g_effect_buffer_index][((lit = timer - 1) - span) * 2]
                = work->clut[g_effect_buffer_index][(lit - span) * 2 + 1] = 0xffff;
        }
        if (span + 1 < g_effect_secondary_current->timer) {
            work->clut[g_effect_buffer_index][((cleared = g_effect_secondary_current->timer - 2) - span) * 2]
                = work->clut[g_effect_buffer_index][(cleared - span) * 2 + 1] = 0;
        }
        rect.x = (g_effect_secondary_current->own_slot_id - 1) * 16 + 1;
        rect.y = 0x1fc;
        rect.w = 6;
        rect.h = 1;
        LoadImage(&rect, (u32*)work->clut[g_effect_buffer_index]);
        caster = unit_get_world_coords_by_runtime_id(g_effect_secondary_current->caster.data.unit.runtime_id);
        target = unit_get_world_coords_by_runtime_id(g_effect_secondary_current->target.record.data.unit.runtime_id);
        setVector(&delta, caster->vx - target->vx, caster->vy - target->vy, caster->vz - target->vz);
        position.vx = target->vx;
        position.vz = target->vz;
        position.vy = target->vy
            - (s32)sprite_get_unit_spritesheet_height_by_runtime_id(
                  g_effect_secondary_current->target.record.data.unit.runtime_id)
                / 2;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;
        scale.vz = 0x140;
        scale.vy = 0x140;
        scale.vx = 0x140;
        parameter = g_effect_secondary_animations[g_effect_secondary_current->animation_id].parameter;
        /* Retail writes z twice and leaves spin.vy unset before RotMatrix reads it. */
        spin.vz = 0;
        spin.vx = 0;
        spin_timer = g_effect_secondary_current->timer;
        spin.vz = spin_timer << 7;
        effect_model_draw(&spin, &position, &angles, &scale, parameter);
        effect = g_effect_secondary_current;
        effect->timer++;
        if (span + 4 < effect->timer) {
            effect->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x13 (Teleport) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 effect_secondary_teleport_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_secondary_current->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_secondary_current->active_count = 0;
        g_effect_secondary_current->timer = 0;
        g_effect_secondary_current->finish_timer = 0;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_secondary_start_group(EFFECT_PARTICLE_GROUP_TELEPORT, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(EFFECT_PARTICLE_GROUP_TELEPORT);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_TELEPORT].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x14 in g_effect_secondary_handlers: a
 * projectile flying in a straight line. It runs the crossbow bolt (the arrow
 * model), the thrown stone and the thrown item animations.
 *
 * Initializing measures the trajectory, takes the flight time from
 * g_projectile_step_count and allocates what the projectile draws
 * with: the polygon buffer of the arrow or stone model, or for a thrown item the
 * shuriken model's buffer (items ITEM_ID_SHURIKEN to ITEM_ID_YAGYU_DARKNESS) or
 * else an effect slot holding the item's icon as a quad. Executing moves 0x8000
 * (8 units in Q12) along the line each frame, aims the model along it and draws
 * it: the arrow at scale 0x40, the stone and the shuriken at 0xc0 and spinning by
 * the frames left, any other item as its icon sprite.
 *
 * With two frames left and a unit hit by the trajectory
 * (g_projectile_hit_unit_id), the target's hit reaction
 * (action_start_projectile_hit_response_by_runtime_id) starts, unless the item is one of
 * the balls or a consumable (ITEM_ID_POTION to ITEM_ID_PHOENIX_DOWN). When the
 * count runs out the icon slot is freed; a unit target whose result is neither
 * MISSED nor NO_EFFECT releases the requested ability effect at vsync interval
 * 2, and otherwise a requested effect is dropped after the hit reaction. The
 * handler then enters Finalizing, whose return of 0 removes it.
 *
 * g_effect_projectile_spin_passes is the count of frames left. result is
 * unset for a phase outside 1-3, and scale and spin for a model other than the
 * arrow or the stone. */
s32 effect_secondary_linear_projectile_handler(void) {
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    SPRT sprite;
    effect_arrow_buffer_t* arrow;
    effect_stone_buffer_t* work12;
    effect_shuriken_buffer_t* work16;
    effect_slot_t* slot;
    s32 parameter;
    s32 item;
    s32 i;
    s32 j;
    s32 result;
    s32 kind;
    POLY_FT4* dst;

    switch (g_effect_secondary_current->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        setVector(&delta, g_projectile_destination.vx - g_projectile_origin.vx,
            g_projectile_destination.vy - g_projectile_origin.vy, g_projectile_destination.vz - g_projectile_origin.vz);
        g_effect_projectile_length
            = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
        g_effect_projectile_delta = delta;
        g_effect_projectile_progress = 0;
        g_effect_projectile_speed = 0x8000;
        g_effect_projectile_spin_passes = g_projectile_step_count;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;

        if (g_effect_secondary_current->allocation != 0) {
            heap_free(g_effect_secondary_current->allocation);
        }

        /* Allocate the drawn model's primitives, one set per display buffer. */
        parameter = g_effect_secondary_animations[g_effect_secondary_current->animation_id].parameter;
        if ((parameter & 0xff) == EFFECT_MODEL_ARROW) {
            arrow = heap_alloc(0xc88);
            g_effect_secondary_current->allocation = arrow;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 4; j++) {
                    SetPolyG3(&arrow->g3[i][j]);
                }
                for (j = 0; j < 13; j++) {
                    SetPolyG4(&arrow->g4[i][j]);
                }
            }
        }

        if ((parameter & 0xff) == EFFECT_MODEL_STONE) {
            work12 = heap_alloc(0x2a0);
            g_effect_secondary_current->allocation = work12;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 12; j++) {
                    SetPolyG3(&work12->g3[i][j]);
                }
            }
        }

        if (parameter == EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            item = g_effect_secondary_current->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                work16 = heap_alloc(0x380);
                g_effect_secondary_current->allocation = work16;
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 16; j++) {
                        SetPolyG3(&work16->g3[i][j]);
                    }
                }
            } else {
                gfx_set_item_icon_sprite(&sprite, item);
                g_effect_secondary_current->slot_ids[0] = effect_slot_alloc();
                slot = &g_effect_slots[g_effect_secondary_current->slot_ids[0]];
                dst = &slot->quads[1];
                SetPolyFT4(&slot->quads[0]);
                slot->quads[0].tpage = 0x1e;
                slot->quads[0].b0 = 0x80;
                slot->quads[0].g0 = 0x80;
                slot->quads[0].r0 = 0x80;
                slot->quads[0].u0 = slot->quads[0].u2 = sprite.u0;
                slot->quads[0].u1 = slot->quads[0].u3 = slot->quads[0].u0 + sprite.w;
                slot->quads[0].v0 = slot->quads[0].v1 = sprite.v0;
                slot->quads[0].v2 = slot->quads[0].v3 = slot->quads[0].v0 + sprite.h;
                slot->quads[0].clut = sprite.clut;
                *dst = slot->quads[0];
            }
        }

        system_set_vsync_interval(1);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_effect_projectile_progress += g_effect_projectile_speed;
        projectile_find_xyz_along_trajectory(
            &g_effect_projectile_delta, g_effect_projectile_length, g_effect_projectile_progress, &position);
        setVector(&position, g_projectile_origin.vx + position.vx, g_projectile_origin.vy + position.vy,
            g_projectile_origin.vz + position.vz);

        /* Aim along the whole origin-to-destination line, not the step taken this frame. */
        delta.vx = g_projectile_destination.vx - g_projectile_origin.vx;
        delta.vz = g_projectile_destination.vz - g_projectile_origin.vz;
        delta.vy = g_projectile_destination.vy - g_projectile_origin.vy;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;

        parameter = g_effect_secondary_animations[g_effect_secondary_current->animation_id].parameter;
        i = parameter & 0xff;
        if (parameter != EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            if (i == EFFECT_MODEL_ARROW) {
                scale.vz = 0x40;
                scale.vy = 0x40;
                scale.vx = 0x40;
                spin.vz = 0;
                spin.vy = 0;
                spin.vx = 0;
            } else if (i == EFFECT_MODEL_STONE) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                setVector(&spin, 0, g_effect_projectile_spin_passes * 256, g_effect_projectile_spin_passes * 128);
            }
            effect_model_draw(&spin, &position, &angles, &scale, parameter);
        } else {
            item = g_effect_secondary_current->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                spin.vy = 0;
                spin.vx = 0;
                spin.vz = -(g_effect_projectile_spin_passes * 256);
                effect_model_draw(&spin, &position, &angles, &scale, EFFECT_MODEL_SHURIKEN);
            } else {
                effect_draw_thrown_item_sprite(&position, item, 0);
            }
        }

        if (g_effect_projectile_spin_passes == 2 && g_projectile_hit_unit_id != -1) {
            kind = g_effect_secondary_current->used_weapon_id;
            if ((kind < ITEM_ID_FIRE_BALL || kind > ITEM_ID_LIGHTNING_BALL)
                && (kind < ITEM_ID_POTION || kind > ITEM_ID_PHOENIX_DOWN)
                && g_effect_secondary_current->target_count != 0) {
                action_start_projectile_hit_response_by_runtime_id(
                    g_effect_secondary_current->caster.data.unit.runtime_id,
                    g_effect_secondary_current->target.record.data.unit.runtime_id);
            }
        }

        if (g_effect_projectile_spin_passes > 0) {
            g_effect_projectile_spin_passes--;
        }
        if (g_effect_projectile_spin_passes == 0) {
            if (g_effect_secondary_current->slot_ids[0] != 0) {
                effect_slot_free(g_effect_secondary_current->slot_ids[0]);
                g_effect_secondary_current->slot_ids[0] = 0;
            }
            if (g_effect_secondary_current->target.record.target_type == EFFECT_TARGET_TYPE_UNIT
                && g_effect_secondary_current->target.record.data.unit.result_animation != EFFECT_HIT_MISSED
                && g_effect_secondary_current->target.record.data.unit.result_animation != EFFECT_HIT_NO_EFFECT) {
                effect_play();
                system_set_vsync_interval(2);
            } else if (effect_get_playback_phase() == EFFECT_PLAYBACK_PHASE_REQUESTED) {
                if (g_effect_secondary_current->target_count != 0) {
                    action_start_projectile_hit_response_by_runtime_id(
                        g_effect_secondary_current->caster.data.unit.runtime_id,
                        g_effect_secondary_current->target.record.data.unit.runtime_id);
                }
                effect_cancel_playback_request();
            }
            g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x15 (Equip Break) in g_effect_secondary_handlers.
 * Same shape as the trap
 * handler, but its particle group is held in a variable, so its spawn_start_frame lookup
 * is an indexed table read. Returns 1 to keep animating.
 */
s32 effect_secondary_equip_break_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    vec3s32_t gravity;
    s32 group = EFFECT_PARTICLE_GROUP_EQUIP_BREAK;
    s32 result;
    s16 finish_timer;

    effect = g_effect_secondary_current;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        effect_secondary_start_group(group, 10);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_save_gravity();
        effect_set_gravity(&gravity);
        effect_secondary_update_active_slots(group);
        effect_restore_inertia_threshold();
        effect_restore_gravity();
        current = g_effect_secondary_current;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_effect_secondary_particle_configs[group].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

enum { EFFECT_RING_CLUT = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 12), EFFECT_RING_SLOT_COUNT = 30 };

/* Secondary effect 0x16 (summon charge): three rings of ten effect slots
 * spinning around the target.
 *
 * Initialization allocates the 30
 * slots and plays sound 0x21; the executing phase grows the radius, spin and
 * brightness with the timer, the finalizing phase widens the rings and fades
 * them out over finish_timer, and both then advance the ring angle, record each
 * ring's newest point and place every slot at a trailing point. Once finish_timer
 * passes 60 the slots are freed and sound 0x21 is paused. Returns 1 while the
 * effect runs.
 */
s32 effect_secondary_summon_charge_handler(void) {
    effect_secondary_data_t* effect;
    effect_ring_state_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x68-byte frame. */
    s32 result;
    s32 i;
    s32 j;
    s32 k;
    u8 id;

    effect = g_effect_secondary_current;
    state = &g_effect_secondary_state.summon_ring;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->index = 0;
        state->radius = 0;
        state->angle_step = 0;
        state->angle = 0;
        effect_resolve_target_world_coords(0, EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &state->center);
        state->brightness = 0;
        state->center.vy -= 24;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                state->points[i][j].vx = state->points[i][j].vy = state->points[i][j].vz = 0;
            }
        }
        for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
            j = g_effect_secondary_current->slot_ids[i] = effect_slot_alloc();
            if (j != 0) {
                slot = &g_effect_slots[j];
                slot->life = effect_slot_spawn_motion(
                    &g_effect_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SUMMON_CHARGE], &slot->motion);
                effect_slot_init_secondary_animation(EFFECT_PARTICLE_GROUP_SUMMON_CHARGE, EFFECT_RING_CLUT, slot);
            }
        }
        g_effect_secondary_current->timer = 0;
        g_effect_secondary_current->finish_timer = 0;
        g_effect_secondary_current->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(SOUND_SFX_SUMMON_CHARGE);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        if (effect->timer < 24) {
            state->radius = effect->timer;
            state->angle_step = effect->timer * 3;
        }
        if (effect->timer < 8) {
            state->brightness = (effect->timer + 1) * 16;
        }
        if (effect->timer < 256) {
            effect->timer++;
        }
        goto update; /* skip the finalizing fade into the shared update */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        if (effect->finish_timer < 61) {
            if (effect->finish_timer & 8) {
                state->radius++;
            }
            state->angle_step++;
            if (effect->finish_timer < 61) {
                state->brightness = (60 - effect->finish_timer) * 4;
                if (effect->finish_timer < 61) {
                    effect->finish_timer++;
                }
            }
        }
    update:
        state->angle += state->angle_step;
        for (i = 0; i < 3; i++) {
            state->points[i][state->index].vx = rcos(i * 0x555 + state->angle) * state->radius / ONE;
            state->points[i][state->index].vz = rsin(i * 0x555 + state->angle) * state->radius / ONE;
        }
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                id = g_effect_secondary_current->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->color.r = slot->color.g = slot->color.b
                        = g_effect_summon_ring_brightness[j] * state->brightness / 128;
                }
            }
        }
        for (i = 0; i < 3; i++) {
            k = state->index;
            for (j = 0; j < 10; j++) {
                id = g_effect_secondary_current->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->motion.position.x = (state->center.vx + state->points[i][k].vx) * ONE;
                    slot->motion.position.y = (state->center.vy + state->points[i][k].vy) * ONE;
                    slot->motion.position.z = (state->center.vz + state->points[i][k].vz) * ONE;
                    effect_slot_update(g_effect_secondary_current->slot_ids[i * 10 + j]);
                }
                k++;
                if (k == 10) {
                    k = 0;
                }
            }
        }
        state->index++;
        if (state->index >= 10) {
            state->index = 0;
        }
        if (g_effect_secondary_current->finish_timer >= 61) {
            for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
                if (g_effect_secondary_current->slot_ids[i] != 0) {
                    effect_slot_free(g_effect_secondary_current->slot_ids[i]);
                    g_effect_secondary_current->slot_ids[i] = 0;
                }
            }
            suzuki_sound_stop_sfx_channels(SOUND_SFX_SUMMON_CHARGE);
            result = 0;
        } else {
            result = 1;
        }
        break;
    }
    return result;
}

/* Run the handler of every active secondary effect once, newest first, and remove
 * each one whose handler returns 0. Returns 0 when none is active.
 *
 * Before the handlers run it takes this frame's ordering table and flips the
 * buffer index their double-buffered primitives use. */
s32 effect_secondary_update(void) {
    effect_secondary_data_t* effect;
    u32 previous_slot_id;

    g_effect_polygon_depth_data = gfx_get_battle_otag();
    g_effect_buffer_index = 1 - g_effect_buffer_index;
    g_effect_secondary_current_id = g_effect_secondary_active_tail;
    if (g_effect_secondary_active_tail == 0) {
        return 0;
    }

    do {
        effect = &g_effect_secondary_data[g_effect_secondary_current_id];
        g_effect_secondary_current = effect;
        previous_slot_id = effect->previous_slot_id;
        if (g_effect_secondary_handlers[effect->function_id]() == 0) {
            effect_secondary_remove(g_effect_secondary_current_id);
        }
        g_effect_secondary_current_id = previous_slot_id;
    } while (previous_slot_id != 0);

    return 1;
}
