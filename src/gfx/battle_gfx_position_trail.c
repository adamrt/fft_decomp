#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Enables the unit's six-copy position trail and seeds every copy with its
 * current position. */
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
