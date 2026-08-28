#include "fft/battle.h"
#include "fft/battle_ai.h"

void battle_unit_apply_poach_morbol_transformation(battle_unit_misc_data_t* unit) {
    u32 flags;
    map_tile_t* tile;

    flags = unit->status_flags_5_6;
    if ((flags & (BATTLE_MISC_STATUS_POACHED | BATTLE_MISC_STATUS_MORBOL)) == 0) {
        return;
    }
    if (flags & BATTLE_MISC_STATUS_POACHED) {
        unit->status_flags_5_6 = flags & ~BATTLE_MISC_STATUS_POACHED;
        battle_gfx_configure_misc_unit_palette_modulation_1f(unit->unit_id);
        return;
    }
    if ((flags & BATTLE_MISC_STATUS_MORBOL) == 0) {
        return;
    }
    unit->status_flags_5_6 = flags & ~BATTLE_MISC_STATUS_MORBOL;
    unit->special_graphic_y_offset = 0;
    unit->spritesheet_id = BATTLE_SPRITESHEET_ID_MORBOL;
    unit->spritesheet_vram_slot = battle_gfx_claim_spritesheet_slot(BATTLE_SPRITESHEET_ID_MORBOL);
    unit->shp_data = battle_gfx_get_spritesheet_shp_data_address(BATTLE_SPRITESHEET_ID_MORBOL);
    unit->seq_data = battle_gfx_get_spritesheet_seq_data_address(BATTLE_SPRITESHEET_ID_MORBOL);
    unit->stored_palette = 0;
    unit->vram_spritesheet_id = unit->spritesheet_vram_slot + 0x14;
    tile = battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    battle_gfx_check_tile_status_palette_mod(unit, tile->flags_06.bits.shadow_mode, 1);
    /* The target reads the u16 facing field signed (lh). */
    battle_unit_store_animation_facing_movement_data(3, (s16)unit->facing, unit);
    battle_unit_modify_entd_unit(unit->battle_data);
    battle_ai_init_unit_abilities(unit->battle_data->misc_unit_id);
}
