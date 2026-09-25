#include "fft/battle.h"

/* Swap a unit's spritesheet for a transformation status, or restore it.
 *
 * Crystal, Treasure, Chicken, and Frog select their corresponding special
 * spritesheets; Crystal and Treasure also clear the other statuses and reset
 * the animation. The first swap away from a regular sheet saves its SHP/SEQ
 * pointers; with no transformation status the saved pointers and the VRAM
 * slot's sheet id are restored. */
void battle_gfx_apply_status_spritesheet_change(battle_unit_misc_data_t* unit, s32 tile_effect_level) {
    u32 flags;
    u16 spritesheet_id;
    u8 original_id;

    flags = unit->status_flags_5_6;
    if (flags & BATTLE_MISC_STATUS_TRANSFORMATION_MASK) {
        if (flags & BATTLE_MISC_STATUS_CRYSTAL) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_CRYSTAL;
            unit->special_graphic_y_offset = 0x60;
            unit->status_flags_5_6 = BATTLE_MISC_STATUS_CRYSTAL;
            unit->status_flags_1_4 = 0;
            battle_unit_store_animation_facing(9, (s16)unit->facing, unit);
        } else if (flags & BATTLE_MISC_STATUS_TREASURE) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_TREASURE;
            unit->special_graphic_y_offset = 0;
            unit->status_flags_5_6 = BATTLE_MISC_STATUS_TREASURE;
            unit->status_flags_1_4 = 0;
            battle_unit_store_animation_facing(0x15, (s16)unit->facing, unit);
        } else if (flags & BATTLE_MISC_STATUS_CHICKEN) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_CHICKEN;
            unit->special_graphic_y_offset = 0;
        } else if (flags & BATTLE_MISC_STATUS_FROG) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_FROG;
            unit->special_graphic_y_offset = 0x30;
        }
        if (unit->spritesheet_id != spritesheet_id) {
            if (unit->spritesheet_id < BATTLE_SPRITESHEET_ID_CRYSTAL) {
                unit->saved_shp_data = unit->shp_data;
                unit->saved_seq_data = unit->seq_data;
            }
            unit->shp_data = battle_gfx_get_spritesheet_shp_data_address(spritesheet_id);
            unit->seq_data = battle_gfx_get_spritesheet_seq_data_address(spritesheet_id);
            unit->spritesheet_id = spritesheet_id;
            battle_gfx_tint_unit_palette_for_weather_and_tile(unit, tile_effect_level, 1);
            battle_unit_set_animation_based_on_status(unit);
            battle_gfx_load_trap_and_unit_frame_parts(unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger,
                unit->animation_frame, unit->encoded_animation);
        }
    } else if (unit->spritesheet_id != g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id) {
        unit->special_graphic_y_offset = 0;
        original_id = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
        unit->shp_data = unit->saved_shp_data;
        unit->seq_data = unit->saved_seq_data;
        unit->spritesheet_id = original_id;
        battle_gfx_check_tile_status_palette_mod(unit, tile_effect_level, 1);
        battle_unit_set_animation_based_on_status(unit);
        battle_gfx_load_trap_and_unit_frame_parts(unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger,
            unit->animation_frame, unit->encoded_animation);
    }
}
