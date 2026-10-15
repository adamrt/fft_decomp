/* Cursor tile projection, wait-direction draw and unit fade-out. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Project the cursor tile to the screen.
 *
 * The tile center (x * 28 + 14, surface height, y * 28 + 14) is raised by the
 * tile's depth, offset by the modify-by-5 or -by-1 conversion depending on
 * surface bit 0x40, and transformed by the battle camera. The projected X and
 * Y are kept in g_battle_cursor_screen_x/g_battle_cursor_screen_y for camera_update_cursor_tile_vector. */
void target_project_cursor_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised) {
    map_tile_t* tile;
    long flag;

    battle_gfx_update_unit_palette_animation();
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    main_util_set_svector(position, g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), 0,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    position->vy = map_calculate_slope_height(position, (u8)g_battle_cursor_z);
    tile = map_get_tile_data_pointer((position->vx / MAP_TILE_SIZE), (position->vz / MAP_TILE_SIZE), g_battle_cursor_z);
    position->vy -= tile->depth_half_height.depth * 12;
    if (tile->surface.depth_bias) {
        camera_convert_screen_coords_modify_by_5(position, raised);
    } else {
        camera_convert_screen_coords_modify_by_1(position, raised);
    }
    RotTrans(raised, projected, &flag);
    g_battle_cursor_screen_x = projected->vx;
    g_battle_cursor_screen_y = projected->vy;
}

/*
 * Tile coordinate calculation with the cursor glow, used while an AT list is
 * active (see target_calculate_tile_coords_and_glow_from_at_list).
 */
void target_calculate_tile_coords_with_cursor_glow(void) {
    SVECTOR secondary;
    SVECTOR tertiary;
    SVECTOR corner0;
    SVECTOR corner1;
    SVECTOR corner2;
    SVECTOR corner3;
    VECTOR primary;
    s32 depth;
    s32 result;
    map_tile_t* tile;

    target_project_cursor_tile_to_screen(&primary, &secondary, &tertiary);
    /* The projected z is the ordering-table depth for the cursor glow. */
    depth = primary.vz / 4;
    battle_gfx_draw_map_selection_cursor(&secondary, main_gfx_get_otag());
    /* The polygon routine reads the same three 16-bit values in renderer x, z, y order. */
    result = target_calculate_cursor_tile_polygon(&secondary, g_battle_cursor_z, &corner0);
    tile = map_get_tile_data_pointer(secondary.vx / MAP_TILE_SIZE, secondary.vz / MAP_TILE_SIZE, g_battle_cursor_z);
    battle_gfx_build_cursor_tile_glow(
        tile->flags.untargetable, result, &corner0, &corner1, &corner2, &corner3, main_gfx_get_otag() + depth);
}

/*
 * Queue the acting unit's wait-direction indicator into the ordering table at
 * the depth of a point 48 screen units above its feet.
 */
void battle_gfx_draw_wait_direction_unit(void) {
    SVECTOR base;
    SVECTOR raised;
    VECTOR transformed;
    long flag;
    battle_unit_t* unit;
    s32 depth;
    u32* ot;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    unit = battle_unit_get_source();
    main_util_set_svector(&base, unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    RotTrans(&base, &transformed, &flag);
    camera_convert_screen_coords_modify_by_1(&unit->world_coords, &raised);
    raised.vy -= 48;
    RotTrans(&raised, &transformed, &flag);
    depth = transformed.vz / 4;
    ot = main_gfx_get_otag();
    battle_gfx_draw_wait_direction_arrows(&base, &ot[depth]);
}

/* Starts a unit's fade-out removal with a color flash.
 *
 * The unit moves to the head of the runtime list, enters BATTLE_UNIT_FADE_OUT_FLASH,
 * loses its shadow and its float and depth offsets, and is drawn with additive
 * blending while its palette fades by (red, green, blue) from the last applied
 * color. `mode` is the amount map_modify_palette steps the fade by, not a
 * color mode. battle_unit_update_fade_out_removal continues from there. */
void battle_gfx_start_unit_fade_out(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        battle_unit_move_to_head(runtime_id);
        unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_FLASH;
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags = (unit->sprite_display_flags & ~BATTLE_SPRITE_DISPLAY_BLEND_MASK)
            | (BATTLE_SPRITE_DISPLAY_BLEND_ADD | BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT);
        battle_gfx_apply_unit_palette_modulation_to_units(MAP_COLOR_MODE_BASE, mode, runtime_id, red, green, blue);
        unit->float_bob_offset = 0;
        unit->depth_height_offset = 0;
    }
}

/* Start a unit's fade-out removal with a red flash: fade step 4 and a red offset
 * of 0x1f. */
void battle_gfx_start_unit_fade_out_red(s32 runtime_id) {
    battle_gfx_start_unit_fade_out(runtime_id, 4, 0x1F, 0, 0);
}
