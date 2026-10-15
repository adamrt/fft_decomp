#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

s32 battle_gfx_open_sp2_file(battle_unit_t* unit, u32* destination) {
    s32 spritesheet_id;
    u8 animation_id;
    s32 index;
    s32 lba;
    s32 byte_length;

    spritesheet_id = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
    animation_id = g_battle_ability_animation_data[unit->sp2_ability_id].attack_animation;
    if (spritesheet_id == BATTLE_SPRITESHEET_ID_STEEL_GIANT) {
        index = animation_id - 0x73;
        lba = g_battle_gfx_worker8_sp2_files[index].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_battle_gfx_worker8_sp2_files[index].size;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    } else {
        lba = g_battle_gfx_sp2_files[spritesheet_id].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_battle_gfx_sp2_files[spritesheet_id].size;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    }
    return -1;
}
