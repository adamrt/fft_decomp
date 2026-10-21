/* Run the resident weapon-hit, projectile, charge and other secondary animations. */

#include "fft/action.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_emitter_data.h"
#include "fft/effect_math.h"
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
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* CLUT of the zodiac poof's particles, an index in the weapons row, for each
 * sprite palette (the secondary effect's parameter). */
extern u8 g_effect_zodiac_poof_clut_indices[];

/* Per-group resident particle emitter records (stride 0x2e). The secondary-effect
 * handlers spawn while spawn_start_frame <= timer < spawn_end_frame and finish
 * once timer passes spawn_start_frame with no live slots;
 * secondary_active_slots_update walks count active slots. The level-up
 * handler seeds group 5's position.y with -8, then lowers it by 3 per frame.
 * Motion bit 0x20 in the level-up and fall-dust rows lies outside the velocity mode mask. */
static effect_emitter_values_t g_secondary_particle_configs[17] = {
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

/* Self-relative animation script table: each 16-bit value is a byte offset from
 * the table's own base to a effect_anim_script_t. Declared as bytes
 * because both uses are byte-based -- the stored pointer is the table's own
 * base plus a byte offset -- and the 16-bit entries are read through a cast
 * at the use site. Declaring it `u16[]` and casting the symbol to `u8*` for
 * the address instead reserves an unused 8-byte frame in
 * slot_secondary_animation_init. */
extern u8 g_effect_secondary_particle_animation_data[];
extern s32 g_projectile_launch_slope_q12;
extern SVECTOR g_projectile_origin;
extern SVECTOR g_projectile_destination;
extern s32 g_projectile_step_count;

extern s32 g_projectile_hit_unit_id;

/* Secondary handler work state. */
/* Per-secondary-slot spawn state for the two-particle death poof. */
typedef struct death_poof_state {
    s32 count;        /* 0x00; slots started so far */
    u16 finish_timer; /* 0x04; reaches 1 when the effect is finished */
    u8 ids[2];        /* 0x06 */
} death_poof_state_t;
typedef char death_poof_state_size_must_be_8[sizeof(death_poof_state_t) == 8 ? 1 : -1];

/* One trail of the spell charge effect: the recent screen positions of the effect slot it follows. */
typedef struct charge_trail {
    DVECTOR points[7]; /* 0x00; screen positions, newest at age % 7 */
    s16 age;           /* 0x1c */
    u8 slot_id;        /* 0x1e */
    u8 _padding_1f;    /* tail padding to 2-byte alignment */
} charge_trail_t;

/* Spell-charge state: the center the trails converge on, two buffers of 96 additive lines (16 trails of 6 segments),
 * its particle ids and counts, and the parameters copied from its g_effect_charge_line_params row; color is the
 * charge's element index. */
typedef struct spell_charge_state {
    s16 x;                     /* 0x0000 */
    s16 y;                     /* 0x0002 */
    DR_MODE mode[2];           /* 0x0004 */
    LINE_G2 lines[2][96];      /* 0x001c */
    charge_trail_t trails[16]; /* 0x0f1c */
    u8 ids_a[2];               /* 0x111c */
    u8 ids_b[14];              /* 0x111e */
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
} spell_charge_state_t;

/* One 14-byte parameter row for the spell-charge secondary effect. */
typedef struct charge_params {
    s16 speed;
    s16 spread_range;
    s16 spread_offset;
    s16 duration;
    s16 radius;
    u8 spawn_interval;
    u8 _unused_0b;
    u8 max_trails;
    u8 fade_row;
} charge_params_t;
typedef char charge_params_size_must_be_14[sizeof(charge_params_t) == 14 ? 1 : -1];

/* g_effect_charge_line_fade_levels holds rows of seven fade steps,
 * indexed flat (row * 7 + step) as the original does. */
enum { EFFECT_CHARGE_LINE_FADE_STEPS = 7 };

/* Summon-charge state: three rings of ten trailing points. */
typedef struct summon_ring_state {
    s32 index;             /* 0x00; newest trail column, 0..9 */
    s32 radius;            /* 0x04 */
    s32 angle;             /* 0x08 */
    s32 angle_step;        /* 0x0c */
    SVECTOR center;        /* 0x10 */
    s32 brightness;        /* 0x18 */
    SVECTOR points[3][10]; /* 0x1c */
} summon_ring_state_t;

/* Zodiac-poof spawn state for the 24-slot particle group 3. */
typedef struct zodiac_poof_state {
    s32 done;   /* 0x00; reaches 1 when the effect is finished */
    s32 count;  /* 0x04; slots started so far */
    u8 ids[24]; /* 0x08 */
} zodiac_poof_state_t;

/* The work area g_effect_secondary_state, shared by the secondary effect handlers; the
 * spell-charge, summon-charge and zodiac-poof handlers each lay it out as
 * their own state. */
typedef union secondary_state {
    spell_charge_state_t charge;
    summon_ring_state_t summon_ring;
    zodiac_poof_state_t zodiac_poof;
} secondary_state_t;

extern secondary_state_t g_effect_secondary_state;
extern death_poof_state_t g_effect_death_poof_states[EFFECT_SECONDARY_RECORD_COUNT];
extern charge_params_t g_effect_charge_line_params[2];

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
extern u16 g_effect_secondary_active_tail;
extern u32 g_effect_secondary_current_id;
extern effect_secondary_callback_t g_effect_secondary_handlers[23];

/* arrow arc */
/* Flight state of effect_secondary_arrow_arc_handler, set in its first phase from the traced trajectory
 * (g_projectile_*): heading, launch pitch, distance along the arc, steps left and the yaw rotation matrix. */
extern s32 g_effect_arrow_arc_length;
extern u16 g_effect_arrow_arc_yaw;
static s16 g_effect_arrow_arc_caster_relative_yaw;
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

/* Start the selected secondary animation on the slot: reset its color and
 * script state and store the 16-bit CLUT value (callers pass CLUTs of
 * BATTLE_MAP_CLUT_ROW_WEAPONS) at 0x88. */
static void slot_secondary_animation_init(s32 group, u16 clut, effect_slot_t* slot) {
    u32 offset;

    slot->clut = clut;
    offset
        = *(u16*)(g_effect_secondary_particle_animation_data + (g_secondary_particle_configs[group].animation_id << 1));
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
static void secondary_group_start(s32 group, s32 clut_index) {
    effect_slot_t* slot;
    s32 spawned;
    s32 i;
    u8 id;

    if (g_secondary_particle_configs[group].count > g_effect_secondary_current->active_count
        && g_effect_secondary_current->timer >= g_secondary_particle_configs[group].spawn_start_frame
        && g_effect_secondary_current->timer < g_secondary_particle_configs[group].spawn_end_frame) {
        for (spawned = 0; spawned < g_secondary_particle_configs[group].spawns_per_frame; spawned++) {
            for (i = 0; i < g_secondary_particle_configs[group].count; i++) {
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
            slot->life = effect_slot_motion_start(&g_secondary_particle_configs[group], &slot->motion);
            slot_secondary_animation_init(group, clut_index + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 0), slot);
            g_effect_secondary_current->active_count++;
            if (g_effect_secondary_current->active_count == g_secondary_particle_configs[group].count) {
                break;
            }
        }
    }
}

/* Advance the current secondary effect's particles: every effect slot in the
 * first count entries of slot_ids, count being the particle group's. A slot that
 * has finished is freed and active_count drops by one. */
static void secondary_active_slots_update(s32 group) {
    s32 i;
    s32 id;

    for (i = 0; i < g_secondary_particle_configs[group].count; i++) {
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
            - (((unit_runtime_facing_quadrant_get(g_effect_secondary_current->caster.data.unit.runtime_id) + 1) & 3)
                << 10);
        g_effect_arrow_launch_slope_q12 = g_projectile_launch_slope_q12;
        g_effect_arrow_arc_launch_angle = ratan2(g_projectile_launch_slope_q12, ONE);
        g_effect_arrow_arc_distance = 0;
        velocity = projectile_arc_launch_velocity_calculate() >> 8;
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
            = -projectile_arc_trajectory_term_calculate(g_effect_arrow_launch_slope_q12, g_effect_arrow_arc_distance);
        g_effect_arrow_arc_distance += g_effect_arrow_arc_distance_step;
        term = -projectile_arc_trajectory_term_calculate(g_effect_arrow_launch_slope_q12, g_effect_arrow_arc_distance);
        next = -projectile_arc_trajectory_term_calculate(
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
typedef struct burst_state {
    s32 frame;    /* 0x00 */
    s32 done;     /* 0x04 */
    s32 count_a;  /* 0x08 */
    u8 ids_a[8];  /* 0x0c; group 0 slots */
    s32 count_b;  /* 0x14 */
    u8 ids_b[16]; /* 0x18; group 1 or 9 slots */
} burst_state_t;

/* Secondary effect function 0x02 in g_effect_secondary_handlers: the burst of a weapon
 * hit (EFFECT_SECONDARY_ANIMATION_WEAPON_HIT). */
s32 effect_secondary_weapon_hit_handler(void) {
    effect_secondary_data_t* effect;
    burst_state_t* state;
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

        state = heap_alloc(sizeof(burst_state_t));
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
        effect_camera_tracks_seed();
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
                for (i = 0; i < g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A].spawns_per_frame;
                    i++) {
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
                    slot->life = effect_slot_motion_start(
                        &g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A], &slot->motion);
                    slot_secondary_animation_init(EFFECT_PARTICLE_GROUP_WEAPON_HIT_A,
                        g_effect_secondary_current->parameter + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 0), slot);
                    state->count_a++;
                    if (state->count_a == 8) {
                        break;
                    }
                }
            }

            if (state->frame < 4 && state->count_b < 16) {
                for (i = 0; i < g_secondary_particle_configs[group].spawns_per_frame; i++) {
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
                    slot->life = effect_slot_motion_start(&g_secondary_particle_configs[group], &slot->motion);
                    slot_secondary_animation_init(
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
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);

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

        effect_inertia_threshold_restore();
        effect_gravity_restore();

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
        secondary_group_start(EFFECT_PARTICLE_GROUP_GOLEM, 13);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(EFFECT_PARTICLE_GROUP_GOLEM);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        current = g_effect_secondary_current;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_GOLEM].spawn_start_frame) {
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
    spell_charge_state_t* state;
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
        g_effect_secondary_current->parameter = effect_element_index_get(effect->parameter);
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
        screen = unit_runtime_world_coords_get(g_effect_secondary_current->caster.data.unit.runtime_id);
        i = sprite_runtime_sheet_height_get(g_effect_secondary_current->caster.data.unit.runtime_id) + 8;
        g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].position.y
            = g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE_13].position.y = -i;
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
            for (i = 0; i < g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].spawns_per_frame; i++) {
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
                slot->life = effect_slot_motion_start(
                    &g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE], &slot->motion);
                slot_secondary_animation_init(
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
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);

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
                        = effect_s32_cos_interpolate(x >> 12, state->x, state->duration, age);
                    fade = 6;
                    state->trails[i].points[head].vy
                        = effect_s32_cos_interpolate(y >> 12, state->y, state->duration, age);
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
        secondary_group_start(EFFECT_PARTICLE_GROUP_RED_ORBS, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(EFFECT_PARTICLE_GROUP_RED_ORBS);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_RED_ORBS].spawn_start_frame) {
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
        secondary_group_start(EFFECT_PARTICLE_GROUP_FALL_DUST, 9);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(EFFECT_PARTICLE_GROUP_FALL_DUST);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_FALL_DUST].spawn_start_frame) {
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
        tile = battle_map_world_tile_get(g_effect_secondary_current->target.record.data.unit.runtime_id);
        g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].position.y
            = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP)
            - unit_runtime_world_coords_get(g_effect_secondary_current->target.record.data.unit.runtime_id)->vy;
        secondary_group_start(EFFECT_PARTICLE_GROUP_SPLASH, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(EFFECT_PARTICLE_GROUP_SPLASH);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].spawn_start_frame) {
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
        secondary_group_start(EFFECT_PARTICLE_GROUP_TRAP, 12);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(EFFECT_PARTICLE_GROUP_TRAP);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_TRAP].spawn_start_frame) {
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
        g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y = -8;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        secondary_group_start(EFFECT_PARTICLE_GROUP_LEVEL_UP, 11);
        {
            /* Addressing through a pointer keeps the target lui/addiu pair. */
            s16* rise = &g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y;
            *rise -= 3;
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(EFFECT_PARTICLE_GROUP_LEVEL_UP);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        current = g_effect_secondary_current;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].spawn_start_frame) {
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
    death_poof_state_t* state;
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

        if (effect->timer >= g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame
            && effect->timer < g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_end_frame) {
            for (spawned = 0; spawned < g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawns_per_frame;
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
                slot->life = effect_slot_motion_start(
                    &g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF], &slot->motion);
                slot_secondary_animation_init(EFFECT_PARTICLE_GROUP_DEATH_POOF, EFFECT_DEATH_POOF_CLUT, slot);
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

        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        for (i = 0; i < EFFECT_DEATH_POOF_SLOT_COUNT; i++) {
            id = state->ids[i];
            if (id != 0 && effect_slot_update(id) == 0) {
                effect_slot_free(id);
                state->ids[i] = 0;
                state->count--;
            }
        }
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        current = g_effect_secondary_current;
        current->timer = current->timer + 1;
        result = state->finish_timer != 1;
        if (state->count == 0
            && current->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame) {
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
    zodiac_poof_state_t* state;
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
        clut_index = g_effect_zodiac_poof_clut_indices[effect->parameter];
        if (effect->timer >= g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame
            && effect->timer < g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawns_per_frame; spawned++) {
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
                slot->life = effect_slot_motion_start(
                    &g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF], &slot->motion);
                slot_secondary_animation_init(EFFECT_PARTICLE_GROUP_ZODIAC_POOF,
                    clut_index + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_WEAPONS, 0), slot);
                state->count++;
                if (state->count == EFFECT_ZODIAC_POOF_SLOT_COUNT) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        for (spawned = 0; spawned < EFFECT_ZODIAC_POOF_SLOT_COUNT; spawned++) {
            id = state->ids[spawned];
            if (id != 0 && effect_slot_update(id) == 0) {
                effect_slot_free(id);
                state->ids[spawned] = 0;
                state->count--;
            }
        }
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        {
            effect_secondary_data_t* current_effect;

            current_effect = g_effect_secondary_current;
            timer = current_effect->timer + 1;
            current_effect->timer = timer;
        }
        done = state->done;
        result = done != 1;
        if (state->count == 0
            && timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame) {
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
        caster = unit_runtime_world_coords_get(g_effect_secondary_current->caster.data.unit.runtime_id);
        target = unit_runtime_world_coords_get(g_effect_secondary_current->target.record.data.unit.runtime_id);
        setVector(&delta, caster->vx - target->vx, caster->vy - target->vy, caster->vz - target->vz);
        position.vx = target->vx;
        position.vz = target->vz;
        position.vy = target->vy
            - sprite_runtime_sheet_height_get(g_effect_secondary_current->target.record.data.unit.runtime_id) / 2;
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
        secondary_group_start(EFFECT_PARTICLE_GROUP_TELEPORT, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(EFFECT_PARTICLE_GROUP_TELEPORT);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        effect = g_effect_secondary_current;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_TELEPORT].spawn_start_frame) {
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
    effect_stone_buffer_t* stone;
    effect_shuriken_buffer_t* shuriken;
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
            stone = heap_alloc(0x2a0);
            g_effect_secondary_current->allocation = stone;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 12; j++) {
                    SetPolyG3(&stone->g3[i][j]);
                }
            }
        }

        if (parameter == EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            item = g_effect_secondary_current->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                shuriken = heap_alloc(0x380);
                g_effect_secondary_current->allocation = shuriken;
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 16; j++) {
                        SetPolyG3(&shuriken->g3[i][j]);
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

        system_vsync_interval_set(1);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_effect_projectile_progress += g_effect_projectile_speed;
        projectile_trajectory_position_calculate(
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
                effect_thrown_item_sprite_draw(&position, item, 0);
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
                effect_playback_start();
                system_vsync_interval_set(2);
            } else if (effect_playback_phase_get() == EFFECT_PLAYBACK_PHASE_REQUESTED) {
                if (g_effect_secondary_current->target_count != 0) {
                    action_start_projectile_hit_response_by_runtime_id(
                        g_effect_secondary_current->caster.data.unit.runtime_id,
                        g_effect_secondary_current->target.record.data.unit.runtime_id);
                }
                effect_playback_request_cancel();
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
        secondary_group_start(group, 10);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_inertia_threshold_save();
        effect_inertia_threshold_set(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        effect_gravity_save();
        effect_gravity_set(&gravity);
        secondary_active_slots_update(group);
        effect_inertia_threshold_restore();
        effect_gravity_restore();
        current = g_effect_secondary_current;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0 && current->timer > g_secondary_particle_configs[group].spawn_start_frame) {
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
    summon_ring_state_t* state;
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
        effect_target_world_coords_resolve(0, EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &state->center);
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
                slot->life = effect_slot_motion_start(
                    &g_secondary_particle_configs[EFFECT_PARTICLE_GROUP_SUMMON_CHARGE], &slot->motion);
                slot_secondary_animation_init(EFFECT_PARTICLE_GROUP_SUMMON_CHARGE, EFFECT_RING_CLUT, slot);
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

    g_effect_polygon_depth_data = gfx_battle_otag_get();
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
