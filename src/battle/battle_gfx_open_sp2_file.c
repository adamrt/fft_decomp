#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* {sector, byte length} SP2 file entries. g_battle_gfx_sp2_files is indexed by
 * spritesheet ID; g_battle_gfx_worker8_sp2_files holds the Worker 8 (spritesheet 0x9a) group,
 * indexed by attack animation - 0x73. */
typedef struct battle_gfx_file_entry {
    s32 lba;
    s32 byte_length;
} battle_gfx_file_entry_t;

extern battle_gfx_file_entry_t g_battle_gfx_sp2_files[];
extern battle_gfx_file_entry_t g_battle_gfx_worker8_sp2_files[];

/* Start loading the SP2 file that holds the unit's pending attack animation.
 *
 * Returns -1 when
 * the spritesheet has no SP2 entry or the animation is still inside the
 * unit's SEQ, otherwise whether the file read request succeeded. */
s32 battle_gfx_open_sp2_file(battle_unit_misc_data_t* unit, void* destination) {
    s32 spritesheet_id;
    u8 animation_id;
    s32 index;
    s32 lba;
    s32 byte_length;

    spritesheet_id = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
    animation_id = g_battle_ability_animation_data[unit->sp2_ability_id].attack_animation;
    if (spritesheet_id == 0x9a) {
        index = animation_id - 0x73;
        lba = g_battle_gfx_worker8_sp2_files[index].lba;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= *(u32*)(unit->seq_data + 4) >> 1) {
            byte_length = g_battle_gfx_worker8_sp2_files[index].byte_length;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    } else {
        lba = g_battle_gfx_sp2_files[spritesheet_id].lba;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= *(u32*)(unit->seq_data + 4) >> 1) {
            byte_length = g_battle_gfx_sp2_files[spritesheet_id].byte_length;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    }
    return -1;
}
