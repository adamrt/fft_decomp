#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"

extern s32 g_main_debug_display_enabled;

/*
 * Event thread started by the AddUnitStart instruction: walks the following
 * script instructions through AddUnitEnd, loading each AddUnit's graphics and
 * processing LoadAttackGraphics before waiting for pending loads.
 */
void world_script_add_unit_start_thread(void) {
    s32 index;
    s32 slot;
    const u8* script;
    battle_stats_t* unit;
    s32 misc_id;
    s16 unit_id;
    u8 opcode;
    u8 flag;

    g_world_thread_task_ids[g_world_thread_current_id][0] = NATIVE_THREAD_TASK_ADD_UNIT;
    slot = 0;
    script = world_thread_get_current_parameter_1();
    while (1) {
        unit_id = world_script_load_halfword(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            world_script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            unit = find_unit_by_id(unit_id, &index);
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
                    world_update_unit_status_and_staged_status_data(slot);
                    battle_gfx_load_unit_graphics_by_battle_id(slot, flag);
                    g_world_unit_graphics_load_pending = 1;
                } else {
                    if (flag == 0) {
                        battle_unit_animate_and_set_enemy_level_data_by_misc_id(misc_id);
                    }
                    if (world_update_unit_status_and_staged_status_data(slot) != 0) {
                        g_world_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                        world_thread_call_on_main_stack(misc_id);
                    }
                }
            }
            while (1) {
                world_thread_yield();
                if (g_world_unit_graphics_load_pending == 0) {
                    break;
                }
                if (g_main_debug_display_enabled != 0) {
                    FntPrint(g_world_text_reading_character_message, slot);
                }
            }
        }
        slot++;
        script += g_world_event_instruction_sizes[script[0]] + 1;
    }
    world_thread_exit_current();
}
