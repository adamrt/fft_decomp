#include "fft/battle.h"
#include "psx/gte.h"

/* flags is passed as a sign-extended word; the u16 mode parameter would
 * zero-extend it, so these calls use a word-mode view. */
#define CONSTRUCT_POLYGONS_WORD_MODE                                                                                   \
    ((void (*)(battle_gfx_sprite_display_t*, s32, s32, s16*, s16, s32, s16*,                                           \
        u32*))battle_gfx_construct_polygon_data_for_units)

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
void battle_gfx_draw_unit_sprite_layers(battle_unit_misc_data_t* unit) {
    SVECTOR position;
    SVECTOR rider_position;
    SVECTOR zoom = { ONE, ONE, ONE };
    battle_unit_misc_data_t* rider;
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

    position.vx = unit->item_get_camera_x;
    position.vy = unit->item_get_camera_y;
    SetGeomScreen(0x200);
    SetGeomOffset(0, 0);
    angle = 0;
    if (unit->otag_depth_index - 1 >= 0x17f) {
        return;
    }
    if (unit->numeric_display_active) {
        battle_gfx_animate_post_action_text(unit, (u16*)&position);
    }
    if (!(((u16)position.vx >= 0x61 && (u16)position.vx < 0x1a0) && ((u16)(position.vy + 0x1f) < 0x12f))
        && unit->spritesheet_id != 0x49) {
        return;
    }
    zoom.vx = g_battle_camera_zoom.vx;
    zoom.vy = g_battle_camera_zoom.vy;
    zoom.vz = g_battle_camera_zoom.vz;
    flags = unit->horizontal_flip_flag ^ unit->sprite_display_flags.half;
    row = unit->layer_priority;
    if (unit->sprite_graphic_trigger == 0) {
        return;
    }
    if (unit->item_ability_display) {
        battle_gfx_init_item_get_rendering((battle_gfx_render_unit_t*)unit);
    }
    if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
        if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        } else if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
            for (i = 0; i < 4; i++) {
                entry = g_battle_gfx_animation_layer_priorities[row][i];
                if (entry != 0) {
                    if (unit->sprite_blocks[entry - 1].trigger == 0) {
                        continue;
                    }
                    display = unit->sprite_blocks[entry - 1].display;
                    CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)display, display->part_count, 0,
                        (s16*)&position, angle, flags, (s16*)&zoom, main_gfx_get_otag() + unit->otag_depth_index);
                } else {
                    display = unit->sprite_display_section;
                    CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)display, 3, 0, (s16*)&position, angle,
                        flags, (s16*)&zoom, main_gfx_get_otag() + unit->otag_depth_index);
                    rider = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
                    if (rider != 0 && rider->ability_in_use != 0) {
                        rider_position.vx = rider->item_get_camera_x;
                        rider_position.vy = rider->item_get_camera_y;
                        rider_flags = rider->sprite_display_flags.half;
                        rider_row = rider->layer_priority;
                        for (j = 0; j < 4; j++) {
                            entry = g_battle_gfx_animation_layer_priorities[rider_row][j];
                            if (entry != 0) {
                                if (rider->sprite_blocks[entry - 1].trigger == 0) {
                                    continue;
                                }
                                rider_display = rider->sprite_blocks[entry - 1].display;
                                CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)rider_display,
                                    rider_display->part_count, 0, (s16*)&rider_position, angle, rider_flags,
                                    (s16*)&zoom, main_gfx_get_otag() + unit->otag_depth_index);
                            } else {
                                rider_display = rider->sprite_display_section;
                                CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)rider_display,
                                    rider_display->part_count, 0, (s16*)&rider_position, angle, rider_flags,
                                    (s16*)&zoom, main_gfx_get_otag() + unit->otag_depth_index);
                            }
                        }
                    }
                    CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)display, display->part_count, 3,
                        (s16*)&position, angle, flags, (s16*)&zoom, main_gfx_get_otag() + unit->otag_depth_index);
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
                CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)display, display->part_count, 0,
                    (s16*)&position, angle, flags, (s16*)&zoom, main_gfx_get_otag() + unit->otag_depth_index);
            } else {
                display = unit->sprite_display_section;
                CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)display, display->part_count, 0,
                    (s16*)&position, angle, (s16)(flags ^ unit->animation_flags), (s16*)&zoom,
                    main_gfx_get_otag() + unit->otag_depth_index);
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
