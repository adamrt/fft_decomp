/* Update unit fades and coordinates and run the distortion animations used for jumps and teleports. */

#include "fft/ability.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/move.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/vector.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

enum {
    ANIMATION_DISTORTION_COUNT = 18 /* the handlers of g_distortion_handlers */
};

/* Phases of the teleport distortion; other distortions reuse the phase field separately. */
typedef enum teleport_phase {
    ANIMATION_TELEPORT_PHASE_START_DEPARTURE = 0,
    ANIMATION_TELEPORT_PHASE_WAIT_DEPARTURE_LOAD = 1,
    ANIMATION_TELEPORT_PHASE_HOLD_DEPARTURE_FLASH = 2,
    ANIMATION_TELEPORT_PHASE_MOVE_TO_DESTINATION = 3,
    ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_LOAD = 4,
    ANIMATION_TELEPORT_PHASE_HOLD_ARRIVAL_DARK = 5,
    ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_FADE = 6,
    ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_END = 7
} teleport_phase_e;

/* The dispatcher ignores the result and the handlers supply no value. The s32
 * callback shape preserves the pulse and teleport handlers' instruction scheduling. */
typedef s32 (*animation_distortion_handler_t)(unit_runtime_t* unit);

static s32 distortion_none(unit_runtime_t* unit);

static s32 distortion_scale_pulse_update(unit_runtime_t* unit);

static s32 distortion_rise(unit_runtime_t* unit);

static s32 distortion_settle(unit_runtime_t* unit);

static s32 distortion_to_target_move(unit_runtime_t* unit);

static s32 distortion_to_own_tile_move(unit_runtime_t* unit);

static s32 distortion_jump_to_target(unit_runtime_t* unit);

static s32 distortion_jump_to_own_tile(unit_runtime_t* unit);

static s32 distortion_to_target_level_move(unit_runtime_t* unit);

static s32 distortion_to_own_tile_level_move(unit_runtime_t* unit);

static s32 distortion_clear(unit_runtime_t* unit);

static s32 distortion_clear_2(unit_runtime_t* unit);

static s32 distortion_jump_start(unit_runtime_t* unit);

static s32 distortion_fall_to_target(unit_runtime_t* unit);

static s32 distortion_teleport(unit_runtime_t* unit);

static s32 distortion_back_away(unit_runtime_t* unit);

static s32 distortion_jump_to_own_tile_faded(unit_runtime_t* unit);

static s32 distortion_jump_rise(unit_runtime_t* unit);

/* One handler for each distortion id; id 0 is skipped by the dispatcher. */
static animation_distortion_handler_t g_distortion_handlers[ANIMATION_DISTORTION_COUNT] = {
    distortion_none,
    distortion_scale_pulse_update,
    distortion_rise,
    distortion_settle,
    distortion_to_target_move,
    distortion_to_own_tile_move,
    distortion_jump_to_target,
    distortion_jump_to_own_tile,
    distortion_to_target_level_move,
    distortion_to_own_tile_level_move,
    distortion_clear,
    distortion_clear_2,
    distortion_jump_start,
    distortion_fall_to_target,
    distortion_teleport,
    distortion_back_away,
    distortion_jump_to_own_tile_faded,
    distortion_jump_rise,
};

/* Starts a unit's fade-out removal with a color flash.
 *
 * The unit moves to the head of the runtime list, enters UNIT_FADE_OUT_FLASH,
 * loses its shadow and its float and depth offsets, and is drawn with additive
 * blending while its palette fades by (red, green, blue) from the last applied
 * color. `fade_step` is the amount battle_map_palette_modify steps the fade by.
 * fade_out_removal_update continues from there. */
void unit_fade_out_start(s32 runtime_id, s32 fade_step, s16 red, s16 green, s16 blue) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit_list_head_move(runtime_id);
        unit->fade_out_phase = UNIT_FADE_OUT_FLASH;
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK)
            | (SPRITE_DISPLAY_BLEND_ADD | SPRITE_DISPLAY_SEMI_TRANSPARENT);
        sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, fade_step, runtime_id, red, green, blue);
        unit->float_bob_offset = 0;
        unit->depth_height_offset = 0;
    }
}

/* Start a unit's fade-out removal with a red flash: fade step 4 and a red offset
 * of 0x1f. */
void unit_fade_out_start_red(s32 runtime_id) {
    unit_fade_out_start(runtime_id, 4, 0x1F, 0, 0);
}

/* Sets the 20.12 fixed-point position from the integer world position. */
void unit_set_real_coords_from_world_coords(unit_runtime_t* unit) {
    unit->real.vx = unit->world_coords.vx * ONE;
    unit->real.vy = unit->world_coords.vy * ONE;
    unit->real.vz = unit->world_coords.vz * ONE;
}

/* Sets the integer world position from the 20.12 fixed-point position. */
void unit_set_world_coords_from_real_coords(unit_runtime_t* unit) {
    unit->world_coords.vx = unit->real.vx / ONE;
    unit->world_coords.vy = unit->real.vy / ONE;
    unit->world_coords.vz = unit->real.vz / ONE;
}

/* Convert map-tile coordinates to tile-centered Q12 world coordinates.
 *
 * Vertical position is derived separately from the unit's terrain and mount
 * state. The stack-backed SVECTOR preserves the target's narrow stores. */
static void real_coords_from_map_coords_set(unit_runtime_t* unit) {
    SVECTOR tile_center;

    tile_center.vx = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    unit->real.vx = tile_center.vx * ONE;
    tile_center.vz = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    unit->real.vz = tile_center.vz * ONE;
    unit->real.vy = unit_height_calculate_from_tile(unit) * ONE;
}

/* Entry 0 of g_distortion_handlers. Never called: id 0 means no distortion
 * animation and animation_distortion_dispatch skips it. */
static s32 distortion_none(unit_runtime_t* unused_unit) {
}

/* Distortion animation 1 (ANIMATION_DISTORTION_SCALE_PULSE). Over 40 frames the sprite's width follows sin(timer) as
 * the timer runs from 0x300 to 0x800, shrinking it to nothing, while its height swells to 2.5 times and back (1 + 1.5
 * sin(angle) for an angle stepping 51 per frame); then both scales return to 1.
 *
 * An ignored s32 result keeps $v0 live at the exit and the dispatch branch's delay
 * slot empty. This callback supplies no return value. */
static s32 distortion_scale_pulse_update(unit_runtime_t* unit) {
    s32 angle;
    s32 stretch;

    switch (unit->distortion_phase) {
    case 0:
        unit->distortion_phase = 1;
        unit->distortion_timer = 0x300;
        unit->distortion_target = 0;
    case 1:
        angle = unit->distortion_target;
        unit->animation.display->scale_x = rsin(unit->distortion_timer);
        stretch = rsin(angle);
        unit->animation.display->scale_y = stretch + stretch / 2 + ONE;
        if ((unit->distortion_timer & 0xfff) != 0x800) {
            unit->distortion_timer = unit->distortion_timer + 32;
            unit->distortion_target = unit->distortion_target + 51;
        } else {
            unit->distortion_phase = unit->distortion_phase + 1;
        }
        break;
    case 2:
        unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        unit->animation.display->scale_x = ONE;
        unit->animation.display->scale_y = ONE;
        break;
    }
}

/* Distortion animation 0x02: raise the unit by 0x24000 (36 world units) in equal
 * steps over distortion_timer frames.
 *
 * Phase 0 replaces the frame count in distortion_timer with the per-frame step
 * and stores the destination height in distortion_target; phase 1 steps until the
 * unit reaches it, then snaps and ends the animation. */
static s32 distortion_rise(unit_runtime_t* unit) {
    s32 step = unit->distortion_timer;
    s32 target = unit->distortion_target;
    s32 height;

    switch (unit->distortion_phase) {
    case 0:
        step = -0x24000 / step;
        target = unit->real.vy + -0x24000;
        unit->distortion_phase = 1;
    case 1:
        if (target < unit->real.vy) {
            unit->real.vy = step + unit->real.vy;
        } else {
            unit->real.vy = target;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        height = unit->real.vy;
        unit->world_coords.vy = height / ONE;
        break;
    }
    unit->distortion_timer = step;
    unit->distortion_target = target;
}

/* Distortion animation 0x03: lower the unit to the ground height under it in
 * equal steps over distortion_timer frames.
 *
 * Phase 0 stores the ground height (Q12) in distortion_target and replaces the
 * frame count in distortion_timer with the per-frame step; phase 1 steps while
 * the unit is above the ground, then snaps and ends the animation. A unit that
 * starts at or below the ground snaps on the first frame. */
static s32 distortion_settle(unit_runtime_t* unit) {
    s32 step = unit->distortion_timer;
    s32 ground = unit->distortion_target;
    s32 height;

    switch (unit->distortion_phase) {
    case 0:
        ground = unit_height_calculate_from_world_coords(unit) * ONE;
        step = (ground - (unit->world_coords.vy * ONE)) / step;
        unit->distortion_phase = unit->distortion_phase + 1;
    case 1:
        if (unit->real.vy < ground) {
            unit->real.vy = step + unit->real.vy;
        } else {
            unit->real.vy = ground;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        height = unit->real.vy;
        unit->world_coords.vy = height / ONE;
        break;
    }
    unit->distortion_timer = step;
    unit->distortion_target = ground;
}

/* Moves a unit toward its action target over distortion_timer frames.
 *
 * Phase 0 picks the destination and splits the remaining distance into equal
 * Q12 steps: during an event the unit's own map tile, else the first entry of
 * the target list, else the recorded action target panel.  Phase 1 applies the
 * steps (height only when allow_height_change is set). */
static void distortion_toward_target_step(unit_runtime_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 saved_target = unit->distortion_target;
    unit_runtime_t* target;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (g_battle_state == BATTLE_STATE_EVENT) {
                velocity.vx = ((unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)) * ONE) - unit->real.vx;
                velocity.vz = ((unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)) * ONE) - unit->real.vz;
                velocity.vy = 0;
            } else if (unit->strike.target_count != 0) {
                target = unit_battle_get(unit->strike.target_list[0]);
                setVector(&velocity, target->real.vx - unit->real.vx, target->real.vy - unit->real.vy,
                    target->real.vz - unit->real.vz);
            } else {
                velocity.vx = (unit->stats->action_command.target_position.vx * BATTLE_MAP_TILE_SIZE
                                  + (BATTLE_MAP_TILE_SIZE / 2) - unit->world_coords.vx)
                    * ONE;
                velocity.vz = (unit->stats->action_command.target_position.vz * BATTLE_MAP_TILE_SIZE
                                  + (BATTLE_MAP_TILE_SIZE / 2) - unit->world_coords.vz)
                    * ONE;
                tile_center.vx = unit->stats->action_command.target_position.vx * BATTLE_MAP_TILE_SIZE
                    + (BATTLE_MAP_TILE_SIZE / 2);
                tile_center.vz = unit->stats->action_command.target_position.vz * BATTLE_MAP_TILE_SIZE
                    + (BATTLE_MAP_TILE_SIZE / 2);
                velocity.vy
                    = (battle_map_slope_height_calculate(&tile_center, unit->stats->action_command.target_position.vy)
                          - unit->world_coords.vy)
                    * ONE;
            }
            velocity.vx /= timer;
            velocity.vy /= timer;
            velocity.vz /= timer;
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            if (allow_height_change) {
                unit->real.vy += velocity.vy;
            }
            unit->real.vz += velocity.vz;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = saved_target;
}

/* Moves a unit to the center of its map tile over distortion_timer frames.
 *
 * Phase 0 splits the distance from its world position to the tile center into equal Q12 steps;
 * phase 1 applies them (height only when allow_height_change is set) and
 * snaps to the tile once the timer runs out. */
static void distortion_to_own_tile_step(unit_runtime_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 saved_target = unit->distortion_target;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vx = (unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2) - unit->world_coords.vx) * ONE;
        velocity.vz = (unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2) - unit->world_coords.vz) * ONE;
        tile_center.vx = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        tile_center.vz = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        velocity.vy = (battle_map_slope_height_calculate(&tile_center, unit->map_layer) - unit->world_coords.vy) * ONE;
        velocity.vx /= timer;
        velocity.vy /= timer;
        velocity.vz /= timer;
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            if (allow_height_change) {
                unit->real.vy += velocity.vy;
            }
            unit->real.vz += velocity.vz;
        } else {
            real_coords_from_map_coords_set(unit);
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = saved_target;
}

/* Distortion animation 0x04: move toward the action target in equal steps,
 * height included (distortion_toward_target_step). */
static s32 distortion_to_target_move(unit_runtime_t* unit) {
    distortion_toward_target_step(unit, 1);
}

/* Distortion animation 0x05: move to the center of the unit's map tile in equal
 * steps, height included (distortion_to_own_tile_step). */
static s32 distortion_to_own_tile_move(unit_runtime_t* unit) {
    distortion_to_own_tile_step(unit, 1);
}

/* Distortion animation 0x08: move toward the action target in equal steps without
 * changing height (distortion_toward_target_step). */
static s32 distortion_to_target_level_move(unit_runtime_t* unit) {
    distortion_toward_target_step(unit, 0);
}

/* Distortion animation 0x09: move to the center of the unit's map tile without
 * changing height. */
static s32 distortion_to_own_tile_level_move(unit_runtime_t* unit) {
    distortion_to_own_tile_step(unit, 0);
}

/* Velocity and duration of a jump arc from `from` to `to`.
 *
 * The arc has two legs under g_move_jump_gravity, each taking
 * sqrt(2 * gravity * height) / gravity frames: one over the height difference
 * plus 24, one over 24. `out->vy` is the negated speed of the first leg; x and
 * y (stored in vz) cover their distances evenly over the returned frame count. */
static s32 distortion_jump_arc_velocity_calculate(const SVECTOR* from, const SVECTOR* to, VECTOR* out) {
    s32 to_height = to->vy;
    s32 from_height = from->vy;
    s32 height = from_height - to_height + 24;
    s32 speed;
    s32 frames;

    height = height * g_move_jump_gravity;
    speed = SquareRoot12(height * 2);
    frames = speed / g_move_jump_gravity;
    out->vy = -speed;
    height = 24;
    height = g_move_jump_gravity * height;
    speed = SquareRoot12(height * 2);
    frames += speed / g_move_jump_gravity;
    out->vx = ((to->vx - from->vx) * ONE) / frames;
    out->vz = ((to->vz - from->vz) * ONE) / frames;
    return frames;
}

/* Distortion animation 0x06: jump along an arc toward the first target's sprite
 * center (its world position raised by half its spritesheet height), or to the
 * action target tile when the unit has no targets.
 *
 * Phase 0 builds the target point and asks
 * distortion_jump_arc_velocity_calculate for the velocity and frame count; a
 * unit without battle data ends the animation instead. Phase 1 applies the
 * velocity to all three real coordinates for that many frames, the height speed
 * gaining g_move_jump_gravity each frame, and ends the animation where the
 * arc leaves the unit. */
static s32 distortion_jump_to_target(unit_runtime_t* unit) {
    SVECTOR target;
    /* The retail frame reserves eight bytes beyond the target vector. */
    u8 unused_target_tail[8];
    VECTOR velocity;
    unit_runtime_t* other;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (unit->strike.target_count != 0) {
                other = unit_battle_get(unit->strike.target_list[0]);
                setVector(&target, other->world_coords.vx,
                    other->world_coords.vy - sprite_runtime_sheet_height_get(other->runtime_id) / 2,
                    other->world_coords.vz);
            } else {
                target.vx = unit->stats->action_command.target_position.vx * BATTLE_MAP_TILE_SIZE
                    + (BATTLE_MAP_TILE_SIZE / 2);
                target.vz = unit->stats->action_command.target_position.vz * BATTLE_MAP_TILE_SIZE
                    + (BATTLE_MAP_TILE_SIZE / 2);
                target.vy = battle_map_slope_height_calculate(&target, unit->stats->action_command.target_position.vy);
            }
            timer = distortion_jump_arc_velocity_calculate(&unit->world_coords, &target, &velocity);
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Distortion animation 0x07: jump along an arc to the center of the unit's own
 * map tile.
 *
 * Phase 0 asks distortion_jump_arc_velocity_calculate for the velocity and frame
 * count from the current world position to the tile center at ground height.
 * Phase 1 applies the velocity for that many frames, the height speed gaining
 * g_move_jump_gravity each frame, then stops the horizontal motion. Phase
 * 2 keeps falling until the unit reaches the ground under it, snaps it to the
 * tile and ends the animation. */
static s32 distortion_jump_to_own_tile(unit_runtime_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * distortion_jump_start; it puts `target` at sp+0x18. */
    SVECTOR unused_10;
    SVECTOR target;
    VECTOR velocity;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        target.vx = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        target.vz = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        target.vy = unit_height_calculate_from_tile(unit);
        timer = distortion_jump_arc_velocity_calculate(&unit->world_coords, &target, &velocity);
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            velocity.vz = 0;
            velocity.vx = 0;
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 2:
        vector_short_set(&target, unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), 0,
            unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
        target.vy = unit_height_calculate_from_world_coords(unit);
        if (unit->world_coords.vy < target.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            real_coords_from_map_coords_set(unit);
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Distortion animation 10 of g_distortion_handlers: ends at once. Animation 11
 * is a second copy of this routine. */
static s32 distortion_clear(unit_runtime_t* unit) {
    unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
}

/* Distortion animation 11: ends at once. The same routine as animation 10, compiled twice. */
static s32 distortion_clear_2(unit_runtime_t* unit) {
    unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
}

/*
 * Distortion animation 0x0c: launch a unit straight up at the start of a jump.
 *
 * Phase 0 picks an initial rise speed of sqrt(2 * gravity * (0x120 - ground y))
 * and the matching frame count, and starts the trailing position copies; phase 1
 * then applies the velocity to the height each frame, decelerating by
 * g_move_jump_gravity, and ends the animation when the count runs out.
 * World y grows downward, so 0x120 - ground y is the altitude 0x120 above the
 * ground under the unit, and the rise covers that distance from wherever the
 * unit starts. `unused_10` reproduces an unreferenced 16-byte frame slot below
 * the velocity copy.
 */
static s32 distortion_jump_start(unit_runtime_t* unit) {
    VECTOR unused_10;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 speed;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        speed = SquareRoot12((-unit_height_calculate_from_world_coords(unit) + 0x120) * g_move_jump_gravity * 2);
        timer = speed / g_move_jump_gravity;
        velocity.vy = -speed;
        sprite_afterimage_init(unit);
        unit->status_flags_5_6 |= UNIT_STATUS_JUMP_HEIGHT_ACTIVE;
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->position_copies_active = 0;
            unit->shadow_graphic_trigger = 0;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            animation_facing_force(ANIMATION_STAND, unit->facing, unit);
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* This retail call passes an unused coordinate-buffer argument in $a1.
 * Preserve the call shape even though the callee reads only the unit. */
#define world_height_from_world_coords_passing_coords                                                                  \
    ((s32 (*)(unit_runtime_t*, const SVECTOR*))unit_height_calculate_from_world_coords)

/* Distortion animation 0x0d: drop the unit straight down until it reaches the
 * ground.
 *
 * Phase 0 sets the fall speed to g_move_jump_gravity, starts the six
 * position copies that draw the sprite's trail and shows the shadow. Phase 1
 * takes the landing height each frame: during an event the ground under the
 * unit's own tile, otherwise the ground under the first target's tile, or under
 * the action target tile when the unit has no target. Above that height the unit
 * falls and gains g_move_jump_gravity per frame; on reaching it the unit
 * snaps to it, the position copies stop and the animation ends.
 *
 * During an event the unit is also centered on its tile, but retail writes the
 * depth coordinate into real.vy instead of real.vz. The height then sits below
 * the ground on the next frame, so an event fall ends on its second frame. */
static s32 distortion_fall_to_target(unit_runtime_t* unit) {
    SVECTOR coords;
    /* Unreferenced 8-byte frame slot above `coords`; it puts `velocity` at
     * sp+0x20, matching the sibling distortion handlers. */
    SVECTOR unused_18;
    VECTOR velocity;
    unit_runtime_t* other;
    unit_stats_t* stats;
    s32 timer;
    s32 x;
    s32 y;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vy = g_move_jump_gravity;
        sprite_afterimage_init(unit);
        unit->shadow_graphic_trigger = 1;
        unit->distortion_phase++;
    case 1:
        if (g_battle_state == BATTLE_STATE_EVENT) {
            unit->strike.target_count = 0;
            x = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            unit->world_coords.vx = x;
            unit->real.vx = x * ONE;
            y = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            unit->world_coords.vz = y;
            /* Retail stores the tile-center depth into real.vy (the height),
             * not real.vz. The first frame adds the fall speed to it and the
             * next one snaps the height to the ground. */
            unit->real.vy = y * ONE;
            vector_short_set(&coords, unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), unit->map_layer,
                unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
            coords.vy = world_height_from_world_coords_passing_coords(unit, &coords);
        } else {
            if (unit->strike.target_count != 0) {
                other = unit_battle_get(unit->strike.target_list[0]);
                vector_short_set(&coords, other->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2),
                    other->map_layer, other->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
            } else {
                stats = unit->stats;
                vector_short_set(&coords,
                    stats->action_command.target_position.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2),
                    stats->action_command.target_position.vy,
                    stats->action_command.target_position.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
            }
            coords.vy = unit_height_calculate_with_caller_coords(unit, &coords);
        }
        if (unit->world_coords.vy < coords.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->real.vy = coords.vy * ONE;
            unit->position_copies_active = 0;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Fill the init record of a teleport effect: the unit is the caster and the only
 * target, and there are no spectators. Both teleport starters use it for every
 * unit, not only for Altima. */
static void teleport_effect_data_init(unit_runtime_t* unit, effect_context_t* teleport_data) {
    u8 runtime_id;
    u8 repeated_runtime_id;

    teleport_data->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    runtime_id = unit->runtime_id;
    teleport_data->target_count = 1;
    teleport_data->targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    teleport_data->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = runtime_id;
    repeated_runtime_id = unit->runtime_id;
    teleport_data->spectator_count = 0;
    teleport_data->targets[0].data.unit.runtime_id = repeated_runtime_id;
}

/* Start the departure effect of a teleport. Altima's two forms request their own
 * ability effect and the result is 1; any other unit gets the teleport secondary
 * effect and the teleport-out sound, and the result is 0. */
static s32 teleport_departure_start(unit_runtime_t* unit) {
    effect_context_t teleport_data;

    teleport_effect_data_init(unit, &teleport_data);
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_ALTIMA_FIRST_FORM:
        effect_ability_animation_set(0, ABILITY_ID_ALTIMA_HOLY_ANGEL_TELEPORT_DEPARTURE, &teleport_data);
        return 1;
    case SPRITE_SHEET_ID_ALTIMA_SECOND_FORM:
        effect_ability_animation_set(0, ABILITY_ID_ALTIMA_ARCH_ANGEL_TELEPORT_DEPARTURE, &teleport_data);
        return 1;
    }
    effect_secondary_teleport_start(unit);
    sound_free_channel_sfx_play(SOUND_SFX_TELEPORT_OUT);
    return 0;
}

/* Start the arrival effect of a teleport. Altima's two forms request their own
 * ability effect and the result is 1; any other unit gets the teleport secondary
 * effect and the teleport-in sound, and the result is 0. */
static s32 teleport_arrival_start(unit_runtime_t* unit) {
    effect_context_t teleport_data;

    teleport_effect_data_init(unit, &teleport_data);
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_ALTIMA_FIRST_FORM:
        effect_ability_animation_set(0, ABILITY_ID_ALTIMA_HOLY_ANGEL_TELEPORT_ARRIVAL, &teleport_data);
        return 1;
    case SPRITE_SHEET_ID_ALTIMA_SECOND_FORM:
        effect_ability_animation_set(0, ABILITY_ID_ALTIMA_ARCH_ANGEL_TELEPORT_ARRIVAL, &teleport_data);
        return 1;
    }
    effect_secondary_teleport_start(unit);
    sound_free_channel_sfx_play(SOUND_SFX_TELEPORT_IN);
    return 0;
}

/* Teleport distortion: begin the departure flash. Altima's forms release the
 * ability effect that teleport_departure_start requested;
 * the sprite switches to additive semi-transparency and a palette modulation of
 * +31 on each color component starts. The distortion moves to its next phase
 * with the timer at 0. */
static void teleport_white_flash_start(unit_runtime_t* unit) {
    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        effect_playback_start();
    }
    unit->sprite_display_flags = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK)
        | (SPRITE_DISPLAY_BLEND_ADD | SPRITE_DISPLAY_SEMI_TRANSPARENT);
    sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 2, unit->runtime_id, 0x1f, 0x1f, 0x1f);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}

/* Teleport distortion: begin the arrival. Altima's forms release the ability
 * effect that teleport_arrival_start requested; the unit's
 * palette is refreshed for the shadow mode of the tile it stands on and a palette
 * modulation of -31 on each color component starts. The distortion moves to its
 * next phase with the timer at 0. */
static void teleport_fade_out_start(unit_runtime_t* unit) {
    battle_map_tile_t* tile;

    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        effect_playback_start();
    }
    tile = battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer);
    sprite_palette_check_tile_status_mod(unit, tile->flags.shading, 1);
    sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 0, unit->runtime_id, -31, -31, -31);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}

/* Distortion animation 14 (TELEPORT), for every unit: advance one frame of the eight-step
 * warp-out / warp-in sequence.
 *
 * Jump table: .rodata 0x80068208, 8 32-bit addresses.
 *
 * An ignored s32 result keeps $v0 live at the epilogue and the switch range
 * check's delay slot empty. This callback supplies no return value. */
static s32 distortion_teleport(unit_runtime_t* unit) {
    unit->distortion_timer++;
    switch (unit->distortion_phase) {
    case ANIMATION_TELEPORT_PHASE_START_DEPARTURE:
        if (teleport_departure_start(unit) == 0) {
            teleport_white_flash_start(unit);
        }
        unit->distortion_phase++;
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_DEPARTURE_LOAD:
        /* The target passes no argument here. */
        if (effect_file_load_update() == 0 && camera_is_active() == 0) {
            teleport_white_flash_start(unit);
        }
        break;
    case ANIMATION_TELEPORT_PHASE_HOLD_DEPARTURE_FLASH:
        if (unit->distortion_timer >= 0x11) {
            sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 4, unit->runtime_id, -31, -31, -31);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_MOVE_TO_DESTINATION:
        if (g_battle_animation_wait_state == 0) {
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->map_layer = unit->movement.destination_layer;
            real_coords_from_map_coords_set(unit);
            unit_set_world_coords_from_real_coords(unit);
            if (teleport_arrival_start(unit) == 0) {
                teleport_fade_out_start(unit);
            }
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_LOAD:
        if (effect_file_load_update() == 0 && camera_is_active() == 0) {
            teleport_fade_out_start(unit);
        }
        break;
    case ANIMATION_TELEPORT_PHASE_HOLD_ARRIVAL_DARK:
        if (unit->distortion_timer >= 0x11) {
            sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 4, unit->runtime_id, 31, 31, 31);
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_FADE:
        if (unit->distortion_timer >= 0x21) {
            sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_RESTORE, 2, unit->runtime_id, 0, 0, 0);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_END:
        if (g_battle_animation_wait_state == 0) {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            unit->sprite_display_flags &= ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
        }
        break;
    }
}

/* Distortion animation 0x0f: slide a unit away from its target over
 * distortion_timer frames (Rush winds up with it).
 *
 * Phase 0 takes the horizontal offset from the first unit in the target list,
 * or from the center of the action target tile, to the unit, reduces it by an
 * eighth, keeps the remainder of it modulo 0xe000 (half a tile, 14 world units,
 * in Q12), and divides that into per-frame velocity. Phase 1 applies that
 * velocity until the timer runs out. Clearing
 * velocity.vy in each branch keeps its division, which GCC folds away when the
 * zero is stored once after the branches. `unused_10` reproduces an
 * unreferenced 8-byte frame slot below the velocity copy. */
static s32 distortion_back_away(unit_runtime_t* unit) {
    s32 unused_10[2];
    VECTOR velocity;
    unit_runtime_t* target;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (unit->strike.target_count != 0) {
                target = unit_battle_get(unit->strike.target_list[0]);
                velocity.vx = unit->real.vx - target->real.vx;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz = unit->real.vz - target->real.vz;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
            } else {
                velocity.vx = (unit->world_coords.vx
                                  - (unit->stats->action_command.target_position.vx * BATTLE_MAP_TILE_SIZE
                                      + (BATTLE_MAP_TILE_SIZE / 2)))
                    * ONE;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz = (unit->world_coords.vz
                                  - (unit->stats->action_command.target_position.vz * BATTLE_MAP_TILE_SIZE
                                      + (BATTLE_MAP_TILE_SIZE / 2)))
                    * ONE;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
            }
            velocity.vx /= timer;
            velocity.vy /= timer;
            velocity.vz /= timer;
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vz += velocity.vz;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Distortion animation 0x10: jump along an arc to the center of the unit's own
 * map tile, darkened for the first part of the flight.
 *
 * Phase 0 computes the arc as distortion_jump_to_own_tile does,
 * hides the shadow, selects quarter-add blending and starts
 * a palette modulation of -31 on each color component. Phase 1 flies until 4
 * frames remain (the threshold kept in distortion_target), then restores the
 * palette. Phase 2 flies the remaining frames and stops the horizontal motion.
 * Phase 3 falls to the ground, snaps the unit to the tile, shows the shadow,
 * clears UNIT_STATUS_JUMP_HEIGHT_ACTIVE and the semi-transparency bit, and
 * ends the animation. */
static s32 distortion_jump_to_own_tile_faded(unit_runtime_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * distortion_jump_to_own_tile; it puts `target` at sp+0x20. */
    SVECTOR unused_18;
    SVECTOR target;
    VECTOR velocity;
    s32 timer;
    s32 threshold;

    timer = unit->distortion_timer;
    threshold = unit->distortion_target;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        target.vx = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        target.vz = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        target.vy = unit_height_calculate_from_tile(unit);
        timer = distortion_jump_arc_velocity_calculate(&unit->world_coords, &target, &velocity);
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags |= SPRITE_DISPLAY_BLEND_ADD_QUARTER | SPRITE_DISPLAY_SEMI_TRANSPARENT;
        sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 1, unit->runtime_id, -31, -31, -31);
        threshold = 4;
        unit->distortion_phase++;
    case 1:
        if (timer > threshold) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_RESTORE, 1, unit->runtime_id, 0, 0, 0);
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 2:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            velocity.vz = 0;
            velocity.vx = 0;
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 3:
        vector_short_set(&target, unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), 0,
            unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
        target.vy = unit_height_calculate_from_world_coords(unit);
        if (unit->world_coords.vy < target.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            real_coords_from_map_coords_set(unit);
            unit->shadow_graphic_trigger = 1;
            unit->status_flags_5_6 &= ~UNIT_STATUS_JUMP_HEIGHT_ACTIVE;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            unit->sprite_display_flags &= ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
        }
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = threshold;
}

/*
 * Distortion animation 0x11: launch a unit upward during a jump landing.
 *
 * Like distortion_jump_start, phase 0 picks a rise speed of
 * sqrt(2 * gravity * (0x120 - ground y)) and a frame count, and phase 1
 * applies it to the height, decelerating by g_move_jump_gravity. Here phase 0 also
 * plays the jump sound, and ends the animation at once when the unit is already
 * at or above 0x120 over the ground; the end of phase 1 only clears the animation id.
 */
static s32 distortion_jump_rise(unit_runtime_t* unit) {
    VECTOR unused_10;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 height;
    s32 speed;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        height = -unit_height_calculate_from_world_coords(unit) + 0x120;
        unit->status_flags_5_6 |= UNIT_STATUS_JUMP_HEIGHT_ACTIVE;
        if (-unit->world_coords.vy >= height) {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
        speed = SquareRoot12(height * g_move_jump_gravity * 2);
        timer = speed / g_move_jump_gravity;
        velocity.vy = -speed;
        sprite_afterimage_init(unit);
        unit_sfx_play(unit, SOUND_SFX_JUMP);
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Run one frame of the unit's distortion animation from
 * g_distortion_handlers. An id past the table reports
 * SYSTEM_ERROR_DISTORTION_ANIMATION. */
void animation_distortion_dispatch(unit_runtime_t* unit) {
    u32 distortion_id = unit->distortion_animation_id;

    if (distortion_id != ANIMATION_DISTORTION_NONE) {
        if (distortion_id < ANIMATION_DISTORTION_COUNT) {
            g_distortion_handlers[distortion_id](unit);
        } else {
            system_error_handle(SYSTEM_ERROR_DISTORTION_ANIMATION);
        }
    }
}
