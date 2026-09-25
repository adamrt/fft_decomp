#include "fft/battle.h"

/*
 * Battle twin of world_script_add_unit_start_thread. Event thread started by
 * AddUnitStart: scans through AddUnitEnd, loading each AddUnit's graphics and
 * processing LoadAttackGraphics before waiting for pending loads.
 */
void battle_script_add_unit_start_thread(void) {
    s32 index;
    s32 slot;
    const u8* script;
    battle_stats_t* unit;
    s32 misc_id;
    s16 unit_id;
    u8 opcode;
    u8 flag;

    g_battle_thread_task_ids[g_battle_current_thread_id][0] = NATIVE_THREAD_TASK_ADD_UNIT;
    slot = 0;
    script = (const u8*)battle_thread_get_current_parameter_1();
    while (1) {
        unit_id = battle_script_load_halfword(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            battle_script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            unit = battle_find_unit_data_pointer_for_entd_unit_id(unit_id, &index);
            if (index == -2 || index >= 0) {
                slot = 0;
                do {
                    if (battle_unit_get_stats_from_battle_id(slot) == unit) {
                        break;
                    }
                    slot++;
                } while (slot < EVENT_UNIT_SLOT_COUNT);
                misc_id = battle_unit_get_misc_id_by_battle_id(slot);
                if (misc_id == -1) {
                    battle_update_unit_status_and_staged_status_data(slot);
                    battle_gfx_load_unit_graphics_by_battle_id(slot, flag);
                    g_battle_unit_graphics_load_pending = 1;
                } else {
                    if (flag == 0) {
                        battle_unit_animate_and_set_enemy_level_data_by_misc_id(misc_id);
                    }
                    if (battle_update_unit_status_and_staged_status_data(slot) != 0) {
                        g_battle_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                        battle_thread_call_on_main_stack(misc_id);
                    }
                }
            }
            while (1) {
                battle_thread_yield();
                if (g_battle_unit_graphics_load_pending == 0) {
                    break;
                }
                if (g_main_debug_display_enabled != 0) {
                    FntPrint(g_battle_text_reading_character_message, slot);
                }
            }
        }
        slot++;
        script += g_battle_script_event_instruction_sizes[script[0]] + 1;
    }
    battle_thread_exit_current();
}
