#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "psx/libgte.h"

/* Queue a unit's sprite layers into the ordering table at its depth.
 *
 * g_battle_gfx_animation_layer_priorities[layer_priority] lists four layers in draw order: 0 is the unit's own
 * sprite display, n > 0 is weapon/effect sprite block n - 1 (skipped while
 * its trigger is clear). A rider (mount state 1) is skipped here; a mount
 * (state 2) draws its first three body parts, then its rider's layers, then
 * the rest of its body. Unmounted units mirror their body sprite by the
 * animation state's flags. The shadow and the position copies follow.
 *
 * The rider loop issues one call per arm; cross-jumping merges them after
 * reload, but the duplicated RTL makes the mount loop large enough that
 * loop.c leaves `row * 16 + g_battle_gfx_animation_layer_priorities` inside it, which is why the target
 * recomputes the table address each iteration and spills `row`. */
void battle_gfx_draw_unit_sprite_layers(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR rider_position;
    SVECTOR zoom = { ONE, ONE, ONE };
    battle_unit_t* rider;
    battle_gfx_sprite_display_data_t* display;
    battle_gfx_sprite_display_data_t* rider_display;
    s32 row;
    s32 rider_row;
    s32 i;
    s32 j;
    s32 entry;
    s16 angle;
    s16 flags;
    s16 rider_flags;

    position.vx = unit->camera_relative_x;
    position.vy = unit->camera_relative_y;
    SetGeomScreen(0x200);
    SetGeomOffset(0, 0);
    angle = 0;
    if (unit->otag_depth_index - 1 >= 0x17f) {
        return;
    }
    if (unit->numeric_display.active) {
        battle_gfx_animate_post_action_text(unit, &position);
    }
    if (!((position.vx >= 0x61 && position.vx <= 0x19f) && (position.vy >= -0x1f && position.vy <= 0x10f))
        && unit->spritesheet_id != BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM) {
        return;
    }
    copyVector(&zoom, &g_camera_zoom);
    flags = unit->horizontal_flip_flag ^ unit->sprite_display_flags;
    row = unit->layer_priority;
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->item_display_active) {
        battle_gfx_init_item_get_rendering(unit);
    }
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        } else if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            for (i = 0; i < 4; i++) {
                entry = g_battle_gfx_animation_layer_priorities[row][i];
                if (entry != 0) {
                    if (unit->sprite_blocks[entry - 1].trigger == 0) {
                        continue;
                    }
                    display = unit->sprite_blocks[entry - 1].display;
                    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle,
                        flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                } else {
                    display = unit->animation.display;
                    battle_gfx_construct_polygon_data_for_units(
                        display, 3, 0, &position, angle, flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                    rider = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                    if (rider != 0 && rider->draw_enabled != 0) {
                        rider_position.vx = rider->camera_relative_x;
                        rider_position.vy = rider->camera_relative_y;
                        rider_flags = rider->sprite_display_flags;
                        rider_row = rider->layer_priority;
                        for (j = 0; j < 4; j++) {
                            entry = g_battle_gfx_animation_layer_priorities[rider_row][j];
                            if (entry != 0) {
                                if (rider->sprite_blocks[entry - 1].trigger == 0) {
                                    continue;
                                }
                                rider_display = rider->sprite_blocks[entry - 1].display;
                                battle_gfx_construct_polygon_data_for_units(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    main_gfx_get_otag() + unit->otag_depth_index);
                            } else {
                                rider_display = rider->animation.display;
                                battle_gfx_construct_polygon_data_for_units(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    main_gfx_get_otag() + unit->otag_depth_index);
                            }
                        }
                    }
                    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 3, &position, angle,
                        flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                }
            }
            if (unit->shadow_graphic_trigger) {
                battle_gfx_draw_unit_shadow(unit, main_gfx_get_otag() + unit->otag_depth_index);
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            entry = g_battle_gfx_animation_layer_priorities[row][i];
            if (entry != 0) {
                if (unit->sprite_blocks[entry - 1].trigger == 0) {
                    continue;
                }
                display = unit->sprite_blocks[entry - 1].display;
                battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle, flags,
                    &zoom, main_gfx_get_otag() + unit->otag_depth_index);
            } else {
                display = unit->animation.display;
                battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle,
                    flags ^ unit->animation.flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
            }
        }
        if (unit->shadow_graphic_trigger) {
            battle_gfx_draw_unit_shadow(unit, main_gfx_get_otag() + unit->otag_depth_index);
        }
    }
    if (unit->position_copies_active) {
        battle_gfx_draw_unit_sprite_position_copies(unit);
    }
}
