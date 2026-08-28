#include "fft/battle.h"
#include "fft/event.h"
#include "fft/thread.h"

/* Run staged status commands for units whose Float or Jump state needs a
 * dedicated update thread. */
void world_process_inflict_status_commands(void) {
    s32 instruction_offset;
    s32 misc_id;
    s32 battle_id;
    s32 thread_id;
    const u8* instruction;
    battle_stats_t* unit;

    instruction_offset = 0;
    while ((instruction_offset
               = world_script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_INFLICT_STATUS))
        != 0) {
        instruction = g_world_event_script;
        if (instruction[instruction_offset] == EVENT_OPCODE_INFLICT_STATUS) {
            misc_id = world_get_misc_id(world_script_load_halfword(instruction + instruction_offset + 1));
            if (misc_id != EVENT_MISC_ID_NONE) {
                battle_id = battle_unit_get_battle_index_by_misc_id(misc_id);
                if (battle_id != 0x7d0) {
                    unit = battle_unit_get_stats_from_battle_id(battle_id);
                    if ((unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FLOAT)]
                            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT))
                        || (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_JUMP)]
                            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP))) {
                        thread_id = world_thread_resolve_id(0x10);
                        world_thread_start(thread_id, world_script_inflict_status_thread);
                        world_thread_set_parameters(
                            thread_id, (s32)(g_world_event_script + instruction_offset + 1), 0, 0);
                        world_thread_wait_until_inactive(thread_id);
                    }
                }
            }
        }
        instruction_offset += g_world_event_instruction_sizes[EVENT_OPCODE_INFLICT_STATUS] + 1;
    }
}
