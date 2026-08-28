#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

typedef struct battle_gfx_sprite_display battle_gfx_sprite_display_t;
extern void battle_gfx_construct_polygon_data_for_units(
    battle_gfx_sprite_display_t* display, s32 end, s32 start, s16* position, s16 angle, u16 mode, s16* scale, u32* ot);

/* flags is passed as a sign-extended word; the u16 mode parameter would
 * zero-extend it, so these calls use a word-mode view. */
#define CONSTRUCT_POLYGONS_WORD_MODE                                                                                   \
    ((void (*)(battle_gfx_sprite_display_t*, s32, s32, s16*, s16, s32, s16*,                                           \
        u32*))battle_gfx_construct_polygon_data_for_units)

/*
 * Draw the unit's sprite once at each of its six display_svectors.
 *
 * Counterpart of battle_gfx_init_position_vector_copies, which seeds the
 * copies; each copy is projected with the battle camera and queued at its own
 * ordering-table depth using the current camera zoom.
 */
void battle_gfx_draw_unit_sprite_position_copies(battle_unit_misc_data_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    VECTOR transformed;
    long flag;
    battle_gfx_sprite_display_data_t* display;
    s32 depth;
    s32 i;
    s16 flags;

    for (i = 0; i < 6; i++) {
        battle_camera_update_matrices(&g_battle_camera_matrix, &g_battle_camera_render_state,
            &g_battle_offset_screen_coords, &g_battle_camera_zoom);
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        RotTrans(&unit->display_svectors[i], &transformed, &flag);
        depth = transformed.vz / 4;
        position.vx = transformed.vx;
        position.vy = transformed.vy;
        flags = ((unit->horizontal_flip_flag ^ unit->sprite_display_flags.half) & 0xff9f) | 1;
        zoom.vx = g_battle_camera_zoom.vx;
        zoom.vy = g_battle_camera_zoom.vy;
        zoom.vz = g_battle_camera_zoom.vz;
        display = unit->sprite_display_section;
        CONSTRUCT_POLYGONS_WORD_MODE((battle_gfx_sprite_display_t*)display, display->part_count, 0, (s16*)&position, 0,
            flags, (s16*)&zoom, main_gfx_get_otag() + depth);
    }
}
