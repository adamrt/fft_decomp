#include "fft/battle_event.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

void script_process_inflict_status_commands(void) {
    s32 instruction_offset;
    s32 runtime_id;
    s32 battle_id;
    s32 thread_id;
    const u8* instruction;
    unit_stats_t* unit;

    instruction_offset = 0;
    while ((instruction_offset = script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_INFLICT_STATUS))
        != 0) {
        instruction = (const u8*)g_script_event_block;
        if (instruction[instruction_offset] == EVENT_OPCODE_INFLICT_STATUS) {
            runtime_id = script_resolve_runtime_id(script_read_s16(instruction + instruction_offset + 1));
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_id = battle_unit_get_battle_id_by_runtime_id(runtime_id);
                if (battle_id != 0x7d0) {
                    unit = battle_unit_get_stats_from_battle_id(battle_id);
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                            & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                            & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                        thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
                        thread_start(thread_id, script_inflict_status_thread);
                        thread_set_parameters(
                            thread_id, (s32)((const u8*)g_script_event_block + instruction_offset + 1), 0, 0);
                        thread_wait_until_inactive(thread_id);
                    }
                }
            }
        }
        instruction_offset += g_script_event_instruction_sizes[EVENT_OPCODE_INFLICT_STATUS] + 1;
    }
}

/* Request a runtime animation, selecting mount/status animations when battle rules require them. */
void unit_set_animation(s32 runtime_id, u32 animation) {
    s32 battle_id;
    unit_stats_t* stats;
    s32 depth;

    if ((script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0)
        && (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) != 0)) {
        battle_id = battle_unit_get_battle_id_by_runtime_id(runtime_id);
        if ((battle_id >= 0) && (animation < BATTLE_SEQUENCE_EVTCHR_SLOT_0)) {
            stats = battle_unit_get_stats_from_battle_id(battle_id);
            depth = map_get_tile_data_value(
                MAP_TILE_DATA_DEPTH, stats->x, stats->position.y, stats->position.higher_elevation);
            if ((stats->mount_info & BATTLE_MOUNT_INFO_ROLE_MASK) != 0) {
                g_thread_call_target = (void (*)(void))battle_unit_set_status_animation_by_runtime_id;
            } else if ((depth & 0xFF) >= 2) {
                g_thread_call_target = (void (*)(void))battle_unit_set_mounted_animation_by_runtime_id;
            } else {
                battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
                return;
            }
            thread_call_on_main_stack(runtime_id);
            return;
        }
    }
    battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
}

/* Faces a unit, and its mount partner when one is linked, in a direction given in
 * sixteenths of a turn. Script variable EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED
 * suppresses the partner's update. */
void unit_set_facing(s32 runtime_id, s32 facing) {
    s32 battle_id;
    unit_stats_t* stats;

    if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0) {
        battle_id = battle_unit_get_battle_id_by_runtime_id(runtime_id);
        if (battle_id >= 0) {
            stats = battle_unit_get_stats_from_battle_id(battle_id);
            if (stats->mount_info != 0) {
                battle_unit_set_animation_from_facing_nibble(
                    battle_unit_get_runtime_id_by_battle_id(stats->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK),
                    facing & 0xFFFF);
            }
        }
    }
    battle_unit_set_animation_from_facing_nibble(runtime_id, facing & 0xFFFF);
}

/* Map script volume 0..96 to driver volume 0..127; values above 96 saturate. */
s32 script_scale_music_volume(s32 volume) {
    if (volume == 0) {
        return 0;
    }
    if (volume >= 96) {
        volume = 127;
    } else {
        volume = (volume * 127) / 96;
    }
    return volume;
}
