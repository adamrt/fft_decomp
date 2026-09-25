#include "fft/battle.h"
#include "psx/types.h"

typedef struct battle_script_jump_instruction {
    u8 pad[2];
    u8 param0;
    u8 param1;
} battle_script_jump_instruction_t;

void battle_script_jump_event_instruction(battle_script_jump_instruction_t* instr) {
    battle_screen_coords_t vec;
    s32 misc_id;
    battle_screen_coords_t* coords;
    battle_stats_t* stats;
    u8 saved;

    misc_id = battle_get_misc_id(battle_script_load_halfword((const u8*)instr));
    if (misc_id != EVENT_MISC_ID_NONE) {
        coords = (battle_screen_coords_t*)battle_unit_get_event_offset_ptr_by_misc_id(misc_id);
        vec.x = -coords->x;
        vec.z = -coords->z;
        vec.y = -coords->y;
        battle_unit_add_event_offset_by_misc_id(misc_id, &vec);
        stats = battle_unit_get_stats_from_battle_id(battle_unit_get_battle_index_by_misc_id(misc_id));
        saved = stats->jump;
        stats->jump = 6;
        battle_unit_prepare_rider_dismount(misc_id, instr->param1, instr->param0 - 1);
        stats->jump = saved;
    }
}
