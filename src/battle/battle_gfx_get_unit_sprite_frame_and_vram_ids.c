#include "fft/battle.h"
#include "psx/types.h"

/*
 * Collect the VRAM sprite identifiers for a unit and return its current SHP
 * frame pointer. Crystal and later special-status sheets share VRAM
 * spritesheet 11.
 */
u8* battle_gfx_get_unit_sprite_frame_and_vram_ids(u32 misc_id, battle_unit_sprite_query_t* out) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit == 0) {
        return 0;
    }

    out->vram_palette_id = unit->vram_palette_id;
    if (unit->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
        out->vram_spritesheet_id = 11;
    } else {
        out->vram_spritesheet_id = unit->vram_spritesheet_id;
    }
    out->graphic_height = g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height;
    out->graphic_y_offset = unit->special_graphic_y_offset;

    if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_RIDER) {
        return unit->shp_data->primary[unit->animation_frame];
    } else {
        return unit->shp_data->secondary[unit->animation_frame];
    }
}
