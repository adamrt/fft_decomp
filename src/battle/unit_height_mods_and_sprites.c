/* Unit height mods and sprites. Its start is an object boundary the read-only data layout places somewhere nearby; its
 * end is a cut chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/types.h"

typedef s16 (*battle_screen_z_halfword_fn_t)(battle_unit_t* unit);

/* Raise a floating unit's height offset, or lower it back to the ground.
 *
 * A floating unit rises up to 12 and then bobs through
 * g_battle_unit_float_bob_offsets unless it is a mount's rider. Otherwise the offset decays to 0,
 * except during the crystal-learn state. The screen/real Z follow the lower
 * of the stored and recalculated heights. Jumping units whose height is
 * controlled by their jump animation are left untouched. */
void battle_unit_increment_or_decrement_height_mod(battle_unit_t* unit) {
    s8 wave[24];
    s16 screen_z;
    u32 phase;

    memcpy(wave, g_battle_unit_float_bob_offsets, sizeof(wave));
    if (unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
        return;
    }
    if (unit->movement.floating) {
        if (unit->depth_height_offset < 12) {
            unit->depth_height_offset += g_main_system_animation_speed;
        } else if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_RIDER) {
            phase = unit->float_bob_phase += g_main_system_animation_speed;
            unit->float_bob_offset = wave[phase % 48 / 2];
        } else {
            unit->float_bob_offset = 0;
        }
    } else if (g_battle_game_state != BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        if (unit->depth_height_offset != 0) {
            unit->depth_height_offset -= g_main_system_animation_speed;
            if (unit->depth_height_offset < 0) {
                unit->depth_height_offset = 0;
            }
            screen_z = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_world_height_from_world_coords)(unit);
            unit->world_coords.vy = screen_z;
            unit->real.vy = screen_z * ONE;
        } else {
            unit->float_bob_offset = 0;
        }
    }
    screen_z = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_world_height_from_world_coords)(unit);
    if (unit->world_coords.vy > screen_z) {
        unit->world_coords.vy = screen_z;
        unit->real.vy = screen_z * ONE;
    }
}

/* Advances one unit's status bubble by g_main_system_animation_speed frames.
 *
 * Every 16 timer ticks the bubble id walks forward to the next status the unit
 * carries, wrapping at 0x16, and the bubble's x/y offsets are chosen from the
 * unit's SHP type and animation.
 *
 * The block-scoped bubble_mask keeps GCC from rotating the loop's leading exit
 * tests to the bottom, and indexing g_battle_gfx_status_bubble_status_masks inside the loop lets loop.c
 * strength-reduce it into the target's pointer walk. */
void battle_gfx_determine_status_bubble_parameters(battle_unit_t* unit) {
    s32 frame;
    s32 bubble;
    u32 status_flags;
    s32 animation;

    for (frame = 0; frame < g_main_system_animation_speed; frame++) {
        bubble = unit->status_bubble_id;
        if (bubble == 9) {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 8) >> 3;
        } else {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 0x10) >> 4;
        }

        if ((unit->status_bubble_timer & 0xF) == 0) {
            status_flags = unit->status_flags_1_4;
            if (((status_flags & BATTLE_UNIT_STATUS_BUBBLE_MASK) == 0)
                && (unit->runtime_id != g_battle_unit_source_runtime_id)) {
                unit->status_bubble_active = 0;
                return;
            }

            for (;;) {
                s32 bubble_mask;

                if (bubble == 0) {
                    if (status_flags & BATTLE_UNIT_STATUS_DEAD) {
                        break;
                    }
                } else if (bubble == 0x14) {
                    if (status_flags & BATTLE_UNIT_STATUS_DEATH_SENTENCE) {
                        break;
                    }
                }
                bubble++;
                if (bubble >= 0x16) {
                    bubble = 0;
                }
                if (bubble == unit->status_bubble_id) {
                    break;
                }
                if (bubble != 0x15) {
                    bubble_mask = g_battle_gfx_status_bubble_status_masks[bubble];
                    if (status_flags & bubble_mask) {
                        break;
                    }
                } else if (unit->runtime_id == g_battle_unit_source_runtime_id) {
                    break;
                }
            }
            unit->status_bubble_id = bubble;

            switch (g_battle_gfx_spritesheet_data[unit->spritesheet_id].shp_id) {
            case BATTLE_SHP_ARUTE:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x46;
                break;
            case BATTLE_SHP_KANZEN:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x78;
                break;
            case BATTLE_SHP_OTHER:
                animation = unit->animation.sequence_id >> 1;
                if (animation == BATTLE_UNIT_ANIMATION_DEAD || animation == BATTLE_UNIT_ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x19;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x19;
                }
                break;
            case BATTLE_SHP_TYPE1:
            case BATTLE_SHP_TYPE2:
                if (map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z)->depth_half_height.depth) {
                    switch (unit->animation.sequence_id >> 1) {
                    case BATTLE_UNIT_ANIMATION_DEAD:
                    case BATTLE_UNIT_ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x23;
                        break;
                    case BATTLE_UNIT_ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                } else {
                    switch (unit->animation.sequence_id >> 1) {
                    case BATTLE_UNIT_ANIMATION_DEAD:
                    case BATTLE_UNIT_ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x19;
                        break;
                    case BATTLE_UNIT_ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                }
                break;
            default:
                animation = unit->animation.sequence_id >> 1;
                if (animation == BATTLE_UNIT_ANIMATION_DEAD || animation == BATTLE_UNIT_ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x1E;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x32;
                }
                break;
            }
        }
        unit->status_bubble_timer++;
    }
}

/*
 * Queue the unit's status bubble into the ordering table.
 *
 * The bubble's texture coordinates are chosen from the status bubble id: the
 * "dying" bubble (id 0) and the status-CT bubble (id 20) show a counter digit,
 * and ids 9, 10 and 21 have a second texture row selected by status_bubble_alternate_row.  The
 * bubble is drawn one tile above the unit's feet and offset by status_bubble_x
 * and status_bubble_y.
 */
void battle_gfx_draw_status_bubble(battle_unit_t* unit, const DVECTOR* unused_screen_coords) {
    SVECTOR position;
    SVECTOR zoom = g_status_bubble_zoom;
    VECTOR transformed;
    SVECTOR base;
    SVECTOR raised;
    long flag;
    battle_gfx_sprite_display_data_t* display;
    s32 bubble_id;
    /* One local for both digit indices: the status-CT arm reuses the dying
     * counter's register, and two separate locals allocate differently. */
    s32 table_index;
    u8 row;
    u8 flag_row;
    u8 default_row;
    s32 depth;
    s16 y;
    u32* otag;

    /* The caller supplies its culling coordinates in a1, but this routine
     * independently projects the bubble and never reads them. */
    (void)unused_screen_coords;

    bubble_id = unit->status_bubble_id;
    display = unit->status_bubble_display;
    if (bubble_id == 21) {
        display->parts[0].u = 0x72;
        display->parts[0].v = 0xB0;
        if (unit->status_bubble_alternate_row != 0) {
            display->parts[0].v = 0xBC;
        }
    } else if (bubble_id == 0) {
        table_index = unit->stats->death_counter;
        if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_RERAISE) {
            display->parts[0].u = g_status_bubble_u[10];
            flag_row = g_status_bubble_v[10];
            display->parts[0].v = flag_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = flag_row + 12;
            }
        } else if (table_index != 0xFF) {
            display->parts[0].u = g_status_bubble_digit_u[table_index];
            display->parts[0].v = g_status_bubble_digit_v[table_index];
        } else {
            display->parts[0].u = g_status_bubble_u[0];
            default_row = g_status_bubble_v[0];
            display->parts[0].v = default_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = default_row + 12;
            }
        }
    } else if (bubble_id == 20) {
        table_index = unit->stats->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] & 3;
        display->parts[0].u = g_status_bubble_digit_u[table_index];
        display->parts[0].v = g_status_bubble_digit_v[table_index];
    } else {
        display->parts[0].u = g_status_bubble_u[bubble_id];
        row = g_status_bubble_v[bubble_id];
        display->parts[0].v = row;
        if (bubble_id == 9 || bubble_id == 10) {
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = row + 12;
            }
        }
    }

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    main_util_set_svector(&base, unit->world_coords.vx + unit->event_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->status_bubble_y,
        unit->world_coords.vz + unit->event_offset.vz);
    RotTrans(&base, &transformed, &unit->gte_flag);
    position.vx = unit->status_bubble_x + (transformed.vx + unit->screen_offset.vx);
    y = transformed.vy + unit->screen_offset.vy;
    position.vy = y;
    if (unit->status_bubble_timer & 0x10) {
        position.vy = y - 1;
    }
    camera_convert_screen_coords_modify_by_1(&base, &raised);
    raised.vy += 12;
    RotTrans(&raised, &transformed, &flag);
    display = unit->status_bubble_display;
    otag = main_gfx_get_otag();
    depth = transformed.vz / 4;
    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, 0, 0, &zoom, otag + depth);
}

/* Per-frame float and status-bubble update of one unit: adjusts its float height
 * except during the four float-jump step phases, then advances its status bubble
 * when one is active. */
void battle_gfx_init_status_bubble(battle_unit_t* unit) {
    u8 phase;

    phase = unit->step_phase;
    if (phase != MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH && phase != MOVE_STEP_PHASE_FLOAT_JUMP_EAST
        && phase != MOVE_STEP_PHASE_FLOAT_JUMP_WEST && phase != MOVE_STEP_PHASE_FLOAT_JUMP_NORTH) {
        battle_unit_increment_or_decrement_height_mod(unit);
    }
    if (unit->status_bubble_active != 0) {
        battle_gfx_determine_status_bubble_parameters(unit);
    }
}

void battle_gfx_init_position_vector_copies(battle_unit_t* unit) {
    s32 i;

    unit->position_copies_active = 1;
    for (i = 0; i < 6; i++) {
        main_util_set_svector(
            &unit->position_copies[i], unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    }
}

/*
 * Draw the unit's sprite once at each of its six position_copies.
 *
 * Counterpart of battle_gfx_init_position_vector_copies, which seeds the
 * copies; each copy is projected with the battle camera and queued at its own
 * ordering-table depth using the current camera zoom.
 */
void battle_gfx_draw_unit_sprite_position_copies(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    VECTOR transformed;
    long flag;
    battle_gfx_sprite_display_data_t* display;
    s32 depth;
    s32 i;
    s16 flags;

    for (i = 0; i < 6; i++) {
        camera_update_matrices(
            &g_camera_matrix, &g_camera_render_state, &g_camera_current_screen_offset, &g_camera_zoom);
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&unit->position_copies[i], &transformed, &flag);
        depth = transformed.vz / 4;
        position.vx = transformed.vx;
        position.vy = transformed.vy;
        flags = ((unit->horizontal_flip_flag ^ unit->sprite_display_flags) & ~BATTLE_SPRITE_DISPLAY_BLEND_MASK)
            | BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
        copyVector(&zoom, &g_camera_zoom);
        display = unit->animation.display;
        battle_gfx_construct_polygon_data_for_units(
            display, display->part_count, 0, &position, 0, flags, &zoom, main_gfx_get_otag() + depth);
    }
}

/* Advances the unit's position trail: each copy takes the next newer one and copy
 * 0 the unit's current position. */
void battle_gfx_shift_and_fill_position_copies(battle_unit_t* unit) {
    s32 index;

    for (index = 5; index > 0; index--) {
        unit->position_copies[index] = unit->position_copies[index - 1];
    }
    battle_unit_copy_draw_coords(unit->position_copies, unit);
}

/* Make the unit's item display show an item's 16x16 icon, centerd on the
 * display's origin. */
void battle_gfx_store_item_display_data(battle_unit_t* unit, u32 item_id) {
    SPRT data;

    gfx_set_item_icon_sprite(&data, item_id);
    unit->item_display->clut = data.clut;
    battle_gfx_store_sprite_display_data(
        unit->item_display, 0, (16 - data.w) / 2 - 8, (16 - data.h) / 2 - 8, data.u0, data.v0, data.w, data.h, 0);
}

/* Queues the unit's item icon at its item-get offset from the unit's screen
 * position, at the unit's ordering-table depth and scale ONE. The x offset is
 * negated unless the sprite is flipped horizontally. */
void battle_gfx_init_item_get_rendering(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    u32* otag;

    if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
        position.vx = unit->item_get_x_offset;
    } else {
        position.vx = -unit->item_get_x_offset;
    }
    position.vy = unit->item_get_y_offset;
    position.vx += unit->camera_relative_x;
    position.vy += unit->camera_relative_y;
    main_util_set_svector(&zoom, ONE, ONE, ONE);
    otag = main_gfx_get_otag();
    otag += unit->otag_depth_index;
    battle_gfx_construct_polygon_data_for_units(
        unit->item_display, unit->item_display->part_count, 0, &position, 0, 0, &zoom, otag);
}
