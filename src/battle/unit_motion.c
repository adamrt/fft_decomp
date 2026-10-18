/* Unit motion, including cursor projection, fades, real and world coordinate conversions, teleport distortion, jump
 * arcs and movement toward targets. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/move.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern animation_distortion_handler_t g_animation_distortion_handlers[ANIMATION_DISTORTION_COUNT];

/* Project the center of the cursor tile, at its surface height less any water depth, to the screen
 * and store the cursor's screen x and y. `position` receives the tile point and `raised` its depth
 * point. It first cycles the tile glow palettes. */
void cursor_project_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised) {
    battle_map_tile_t* tile;
    s32 flag;

    cursor_cycle_tile_glow_palettes();
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    vector_set_svector(position, g_cursor_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), 0,
        g_cursor_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
    position->vy = battle_map_calculate_slope_height(position, (u8)g_cursor_z);
    tile = battle_map_tile_get_pointer(
        (position->vx / BATTLE_MAP_TILE_SIZE), (position->vz / BATTLE_MAP_TILE_SIZE), g_cursor_z);
    position->vy -= tile->depth_half_height.depth * 12;
    if (tile->surface.depth_bias) {
        camera_get_tile_depth_point_biased(position, raised);
    } else {
        camera_get_tile_depth_point_unbiased(position, raised);
    }
    RotTrans(raised, projected, &flag);
    g_cursor_screen_x = projected->vx;
    g_cursor_screen_y = projected->vy;
}

/* Project the cursor tile and draw the map cursor over it with the glow quad on the tile surface;
 * cursor_update calls it while g_cursor_visible is set. The tile's
 * untargetable flag picks the glow's palette. */
void cursor_draw_with_tile_glow(void) {
    SVECTOR position;
    SVECTOR raised;
    SVECTOR corner0;
    SVECTOR corner1;
    SVECTOR corner2;
    SVECTOR corner3;
    VECTOR projected;
    s32 depth;
    s32 shape;
    battle_map_tile_t* tile;

    cursor_project_tile_to_screen(&projected, &position, &raised);
    /* The projected z is the ordering-table depth for the cursor glow. */
    depth = projected.vz / 4;
    cursor_draw_map_selection(&position, gfx_get_battle_otag());
    /* The polygon routine reads the same three 16-bit values in renderer x, z, y order. */
    shape = cursor_calculate_tile_polygon(&position, g_cursor_z, &corner0);
    tile = battle_map_tile_get_pointer(
        position.vx / BATTLE_MAP_TILE_SIZE, position.vz / BATTLE_MAP_TILE_SIZE, g_cursor_z);
    cursor_build_tile_glow(
        tile->flags.untargetable, shape, &corner0, &corner1, &corner2, &corner3, gfx_get_battle_otag() + depth);
}

/*
 * Queue the acting unit's wait-direction indicator into the ordering table at
 * the depth of a point 48 screen units above its feet.
 */
void unit_facing_draw_wait_direction(void) {
    SVECTOR base;
    SVECTOR raised;
    VECTOR transformed;
    s32 flag;
    unit_t* unit;
    s32 depth;
    u32* ot;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    unit = unit_get_source();
    vector_set_svector(&base, unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    /* The second RotTrans overwrites this result unread. */
    RotTrans(&base, &transformed, &flag);
    camera_get_tile_depth_point_unbiased(&unit->world_coords, &raised);
    raised.vy -= 48;
    RotTrans(&raised, &transformed, &flag);
    depth = transformed.vz / 4;
    ot = gfx_get_battle_otag();
    unit_facing_draw_wait_arrows(&base, &ot[depth]);
}

/* Starts a unit's fade-out removal with a color flash.
 *
 * The unit moves to the head of the runtime list, enters UNIT_FADE_OUT_FLASH,
 * loses its shadow and its float and depth offsets, and is drawn with additive
 * blending while its palette fades by (red, green, blue) from the last applied
 * color. `mode` is the amount battle_map_palette_modify steps the fade by, not a
 * color mode. unit_update_fade_out_removal continues from there. */
void unit_fade_out_start(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit_move_to_head(runtime_id);
        unit->fade_out_phase = UNIT_FADE_OUT_FLASH;
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK)
            | (SPRITE_DISPLAY_BLEND_ADD | SPRITE_DISPLAY_SEMI_TRANSPARENT);
        sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, mode, runtime_id, red, green, blue);
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
void unit_set_real_coords_from_world_coords(unit_t* unit) {
    unit->real.vx = unit->world_coords.vx * ONE;
    unit->real.vy = unit->world_coords.vy * ONE;
    unit->real.vz = unit->world_coords.vz * ONE;
}

/* Sets the integer world position from the 20.12 fixed-point position. */
void unit_set_world_coords_from_real_coords(unit_t* unit) {
    unit->world_coords.vx = unit->real.vx / ONE;
    unit->world_coords.vy = unit->real.vy / ONE;
    unit->world_coords.vz = unit->real.vz / ONE;
}

/* Convert map-tile coordinates to tile-centered Q12 world coordinates.
 *
 * Vertical position is derived separately from the unit's terrain and mount
 * state. The stack-backed SVECTOR preserves the target's narrow stores. */
static void unit_set_real_coords_from_map_coords(unit_t* unit) {
    SVECTOR tile_center;

    tile_center.vx = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    unit->real.vx = tile_center.vx * ONE;
    tile_center.vz = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    unit->real.vz = tile_center.vz * ONE;
    unit->real.vy = unit_height_calculate_from_tile(unit) * ONE;
}

/* Entry 0 of g_animation_distortion_handlers. Never called: id 0 means no distortion
 * animation and animation_distortion_dispatch skips it. */
s32 animation_distortion_none(unit_t* unused_unit) {
}

/* Distortion animation 1 (ANIMATION_DISTORTION_SCALE_PULSE; nothing starts it, and the
 * teleport is animation 14). Over 40 frames the sprite's width follows sin(timer) as the timer
 * runs from 0x300 to 0x800, shrinking it to nothing, while its height swells to 2.5 times and back
 * (1 + 1.5 sin(angle) for an angle stepping 51 per frame); then both scales return to 1.
 *
 * An ignored s32 result keeps $v0 live at the exit and the dispatch branch's delay
 * slot empty. This callback supplies no return value. */
s32 animation_distortion_scale_pulse(unit_t* unit) {
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
s32 animation_distortion_rise(unit_t* unit) {
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
s32 animation_distortion_settle(unit_t* unit) {
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
static void animation_distortion_step_toward_target(unit_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 target_z = unit->distortion_target;
    unit_t* target;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (g_game_state == GAME_STATE_EVENT) {
                velocity.vx = ((unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)) * ONE) - unit->real.vx;
                velocity.vz = ((unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)) * ONE) - unit->real.vz;
                velocity.vy = 0;
            } else if (unit->strike.target_count != 0) {
                target = unit_get_by_battle_id(unit->strike.target_list[0]);
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
                    = (battle_map_calculate_slope_height(&tile_center, unit->stats->action_command.target_position.vy)
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
    unit->distortion_target = target_z;
}

/* Moves a unit to the center of its map tile over distortion_timer frames.
 *
 * Phase 0 splits the distance from its world position to the tile center into equal Q12 steps;
 * phase 1 applies them (height only when allow_height_change is set) and
 * snaps to the tile once the timer runs out. */
static void animation_distortion_step_to_own_tile(unit_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 target_z = unit->distortion_target;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vx = (unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2) - unit->world_coords.vx) * ONE;
        velocity.vz = (unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2) - unit->world_coords.vz) * ONE;
        tile_center.vx = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        tile_center.vz = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        velocity.vy = (battle_map_calculate_slope_height(&tile_center, unit->map_z) - unit->world_coords.vy) * ONE;
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
            unit_set_real_coords_from_map_coords(unit);
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = target_z;
}

/* Distortion animation 0x04: move toward the action target in equal steps,
 * height included (animation_distortion_step_toward_target). */
s32 animation_distortion_move_to_target(unit_t* unit) {
    animation_distortion_step_toward_target(unit, 1);
}

/* Distortion animation 0x05: move to the center of the unit's map tile in equal
 * steps, height included (animation_distortion_step_to_own_tile). */
s32 animation_distortion_move_to_own_tile(unit_t* unit) {
    animation_distortion_step_to_own_tile(unit, 1);
}

/* Distortion animation 0x08: move toward the action target in equal steps without
 * changing height (animation_distortion_step_toward_target). */
s32 animation_distortion_move_to_target_level(unit_t* unit) {
    animation_distortion_step_toward_target(unit, 0);
}

/* Distortion animation 0x09: move to the center of the unit's map tile without
 * changing height. */
s32 animation_distortion_move_to_own_tile_level(unit_t* unit) {
    animation_distortion_step_to_own_tile(unit, 0);
}

/* Velocity and duration of a jump arc from `from` to `to`.
 *
 * The arc has two legs under g_move_jump_gravity, each taking
 * sqrt(2 * gravity * height) / gravity frames: one over the height difference
 * plus 24, one over 24. `out->vy` is the negated speed of the first leg; x and
 * y (stored in vz) cover their distances evenly over the returned frame count. */
static s32 animation_distortion_calculate_jump_arc_velocity(const SVECTOR* from, const SVECTOR* to, VECTOR* out) {
    s32 to_z = to->vy;
    s32 from_z = from->vy;
    s32 height = from_z - to_z + 24;
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
 * animation_distortion_calculate_jump_arc_velocity for the velocity and frame count; a
 * unit without battle data ends the animation instead. Phase 1 applies the
 * velocity to all three real coordinates for that many frames, the height speed
 * gaining g_move_jump_gravity each frame, and ends the animation where the
 * arc leaves the unit. */
s32 animation_distortion_jump_to_target(unit_t* unit) {
    SVECTOR target;
    /* The retail frame reserves eight bytes beyond the target vector. */
    u8 unused_target_tail[8];
    VECTOR velocity;
    unit_t* other;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (unit->strike.target_count != 0) {
                other = unit_get_by_battle_id(unit->strike.target_list[0]);
                setVector(&target, other->world_coords.vx,
                    other->world_coords.vy - sprite_get_unit_spritesheet_height_by_runtime_id(other->runtime_id) / 2,
                    other->world_coords.vz);
            } else {
                target.vx = unit->stats->action_command.target_position.vx * BATTLE_MAP_TILE_SIZE
                    + (BATTLE_MAP_TILE_SIZE / 2);
                target.vz = unit->stats->action_command.target_position.vz * BATTLE_MAP_TILE_SIZE
                    + (BATTLE_MAP_TILE_SIZE / 2);
                target.vy = battle_map_calculate_slope_height(&target, unit->stats->action_command.target_position.vy);
            }
            timer = animation_distortion_calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
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
 * Phase 0 asks animation_distortion_calculate_jump_arc_velocity for the velocity and frame
 * count from the current world position to the tile center at ground height.
 * Phase 1 applies the velocity for that many frames, the height speed gaining
 * g_move_jump_gravity each frame, then stops the horizontal motion. Phase
 * 2 keeps falling until the unit reaches the ground under it, snaps it to the
 * tile and ends the animation. */
s32 animation_distortion_jump_to_own_tile(unit_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * animation_distortion_jump_start; it puts `target` at sp+0x18. */
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
        timer = animation_distortion_calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
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
        vector_set_svector(&target, unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), 0,
            unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
        target.vy = unit_height_calculate_from_world_coords(unit);
        if (unit->world_coords.vy < target.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit_set_real_coords_from_map_coords(unit);
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        break;
    }
    unit->velocity = velocity;
    unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Distortion animation 10 of g_animation_distortion_handlers: ends at once. Animation 11
 * is a second copy of this routine. */
s32 animation_distortion_clear(unit_t* unit) {
    unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
}

/* Distortion animation 11: ends at once. The same routine as animation 10, compiled twice. */
s32 animation_distortion_clear_2(unit_t* unit) {
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
s32 animation_distortion_jump_start(unit_t* unit) {
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
            animation_force_facing(ANIMATION_STAND, unit->facing, unit);
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
    ((s32 (*)(unit_t*, const SVECTOR*))unit_height_calculate_from_world_coords)

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
s32 animation_distortion_fall_to_target(unit_t* unit) {
    SVECTOR coords;
    /* Unreferenced 8-byte frame slot above `coords`; it puts `velocity` at
     * sp+0x20, matching the sibling distortion handlers. */
    SVECTOR unused_18;
    VECTOR velocity;
    unit_t* other;
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
        if (g_game_state == GAME_STATE_EVENT) {
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
            vector_set_svector(&coords, unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), unit->map_z,
                unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
            coords.vy = world_height_from_world_coords_passing_coords(unit, &coords);
        } else {
            if (unit->strike.target_count != 0) {
                other = unit_get_by_battle_id(unit->strike.target_list[0]);
                vector_set_svector(&coords, other->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2),
                    other->map_z, other->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
            } else {
                stats = unit->stats;
                vector_set_svector(&coords,
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
static void animation_teleport_init_effect_data(unit_t* unit, effect_context_t* teleport_data) {
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
static s32 animation_teleport_start_departure(unit_t* unit) {
    effect_context_t teleport_data;

    animation_teleport_init_effect_data(unit, &teleport_data);
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_ALTIMA_FIRST_FORM:
        effect_set_ability_animation(0, ABILITY_ID_ALTIMA_HOLY_ANGEL_TELEPORT_DEPARTURE, &teleport_data);
        return 1;
    case SPRITE_SHEET_ID_ALTIMA_SECOND_FORM:
        effect_set_ability_animation(0, ABILITY_ID_ALTIMA_ARCH_ANGEL_TELEPORT_DEPARTURE, &teleport_data);
        return 1;
    }
    effect_secondary_set_teleport(unit);
    sound_play_sfx_on_free_channel(SOUND_SFX_TELEPORT_OUT);
    return 0;
}

/* Start the arrival effect of a teleport. Altima's two forms request their own
 * ability effect and the result is 1; any other unit gets the teleport secondary
 * effect and the teleport-in sound, and the result is 0. */
static s32 animation_teleport_start_arrival(unit_t* unit) {
    effect_context_t teleport_data;

    animation_teleport_init_effect_data(unit, &teleport_data);
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_ALTIMA_FIRST_FORM:
        effect_set_ability_animation(0, ABILITY_ID_ALTIMA_HOLY_ANGEL_TELEPORT_ARRIVAL, &teleport_data);
        return 1;
    case SPRITE_SHEET_ID_ALTIMA_SECOND_FORM:
        effect_set_ability_animation(0, ABILITY_ID_ALTIMA_ARCH_ANGEL_TELEPORT_ARRIVAL, &teleport_data);
        return 1;
    }
    effect_secondary_set_teleport(unit);
    sound_play_sfx_on_free_channel(SOUND_SFX_TELEPORT_IN);
    return 0;
}

/* Teleport distortion: begin the departure flash. Altima's forms release the
 * ability effect that animation_teleport_start_departure requested;
 * the sprite switches to additive semi-transparency and a palette modulation of
 * +31 on each color component starts. The distortion moves to its next phase
 * with the timer at 0. */
static void animation_teleport_start_white_flash(unit_t* unit) {
    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        effect_play();
    }
    unit->sprite_display_flags = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK)
        | (SPRITE_DISPLAY_BLEND_ADD | SPRITE_DISPLAY_SEMI_TRANSPARENT);
    sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 2, unit->runtime_id, 0x1f, 0x1f, 0x1f);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}

/* Teleport distortion: begin the arrival. Altima's forms release the ability
 * effect that animation_teleport_start_arrival requested; the unit's
 * palette is refreshed for the shadow mode of the tile it stands on and a palette
 * modulation of -31 on each color component starts. The distortion moves to its
 * next phase with the timer at 0. */
static void animation_teleport_start_fade_out(unit_t* unit) {
    battle_map_tile_t* tile;

    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        effect_play();
    }
    tile = battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_z);
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
s32 animation_distortion_teleport(unit_t* unit) {
    unit->distortion_timer++;
    switch (unit->distortion_phase) {
    case 0:
        if (animation_teleport_start_departure(unit) == 0) {
            animation_teleport_start_white_flash(unit);
        }
        unit->distortion_phase++;
        break;
    case 1:
        /* The target passes no argument here. */
        if (effect_file_update_load() == 0 && camera_is_active() == 0) {
            animation_teleport_start_white_flash(unit);
        }
        break;
    case 2:
        if (unit->distortion_timer >= 0x11) {
            sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 4, unit->runtime_id, -31, -31, -31);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case 3:
        if (g_state_animation_continue_check == 0) {
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->map_z = unit->movement.destination_z;
            unit_set_real_coords_from_map_coords(unit);
            unit_set_world_coords_from_real_coords(unit);
            if (animation_teleport_start_arrival(unit) == 0) {
                animation_teleport_start_fade_out(unit);
            }
            unit->distortion_phase++;
        }
        break;
    case 4:
        if (effect_file_update_load() == 0 && camera_is_active() == 0) {
            animation_teleport_start_fade_out(unit);
        }
        break;
    case 5:
        if (unit->distortion_timer >= 0x11) {
            sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 4, unit->runtime_id, 31, 31, 31);
            unit->distortion_phase++;
        }
        break;
    case 6:
        if (unit->distortion_timer >= 0x21) {
            sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_RESTORE, 2, unit->runtime_id, 0, 0, 0);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case 7:
        if (g_state_animation_continue_check == 0) {
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
s32 animation_distortion_back_away(unit_t* unit) {
    s32 unused_10[2];
    VECTOR velocity;
    unit_t* target;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (unit->strike.target_count != 0) {
                target = unit_get_by_battle_id(unit->strike.target_list[0]);
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
 * Phase 0 computes the arc as animation_distortion_jump_to_own_tile does,
 * hides the shadow, selects quarter-add blending and starts
 * a palette modulation of -31 on each color component. Phase 1 flies until 4
 * frames remain (the threshold kept in distortion_target), then restores the
 * palette. Phase 2 flies the remaining frames and stops the horizontal motion.
 * Phase 3 falls to the ground, snaps the unit to the tile, shows the shadow,
 * clears UNIT_STATUS_JUMP_HEIGHT_ACTIVE and the semi-transparency bit, and
 * ends the animation. */
s32 animation_distortion_jump_to_own_tile_faded(unit_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * animation_distortion_jump_to_own_tile; it puts `target` at sp+0x20. */
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
        timer = animation_distortion_calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
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
        vector_set_svector(&target, unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), 0,
            unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
        target.vy = unit_height_calculate_from_world_coords(unit);
        if (unit->world_coords.vy < target.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit_set_real_coords_from_map_coords(unit);
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
 * Like animation_distortion_jump_start, phase 0 picks a rise speed of
 * sqrt(2 * gravity * (0x120 - ground y)) and a frame count, and phase 1
 * applies it to the height, decelerating by g_move_jump_gravity. Here phase 0 also
 * plays the jump sound, and ends the animation at once when the unit is already
 * at or above 0x120 over the ground; the end of phase 1 only clears the animation id.
 */
s32 animation_distortion_jump_rise(unit_t* unit) {
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
        unit_play_sfx(unit, SOUND_SFX_JUMP);
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
 * g_animation_distortion_handlers. An id past the table reports
 * SYSTEM_ERROR_DISTORTION_ANIMATION. */
void animation_distortion_dispatch(unit_t* unit) {
    u32 distortion_id = unit->distortion_animation_id;

    if (distortion_id != ANIMATION_DISTORTION_NONE) {
        if (distortion_id < ANIMATION_DISTORTION_COUNT) {
            g_animation_distortion_handlers[distortion_id](unit);
        } else {
            system_handle_error(SYSTEM_ERROR_DISTORTION_ANIMATION);
        }
    }
}
