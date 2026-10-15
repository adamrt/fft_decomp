#include "fft/battle_gfx.h"
#include "psx/types.h"

/*
 * Give a unit's CLUT, texture page, graphic height and v offset, and return
 * the SHP frame it is showing: the submerged one for a rider. The crystal
 * and later status sheets share the OTHER texture page.
 */
battle_gfx_source_frame_t* battle_gfx_get_unit_sprite_frame_and_vram_ids(
    u32 runtime_id, battle_unit_sprite_query_t* out) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit == 0) {
        return 0;
    }

    out->clut = unit->clut;
    if (unit->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
        out->tpage = BATTLE_GFX_TPAGE_OTHER;
    } else {
        out->tpage = unit->tpage;
    }
    out->graphic_height = g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height;
    out->graphic_y_offset = unit->special_graphic_y_offset;

    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_RIDER) {
        return unit->animation.shp->frames[unit->animation.frame];
    } else {
        return unit->animation.shp->submerged_frames[unit->animation.frame];
    }
}
