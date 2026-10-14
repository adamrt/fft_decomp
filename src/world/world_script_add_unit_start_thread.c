#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"

/*
 * Event thread started by the AddUnitStart instruction: walks the following
 * script instructions through AddUnitEnd, loading each AddUnit's graphics and
 * processing LoadAttackGraphics before waiting for pending loads.
 */
void world_script_add_unit_start_thread(void) {
    s32 index;
    s32 slot;
    const u8* script;
    unit_stats_t* unit;
    s32 runtime_id;
    s16 unit_id;
    u8 opcode;
    u8 flag;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_ADD_UNIT;
    slot = 0;
    script = (const u8*)world_thread_get_current_parameter_1();
    while (1) {
        unit_id = script_read_s16(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &index);
            if (index == -2 || index >= 0) {
                for (slot = 0; slot < EVENT_UNIT_SLOT_COUNT; slot++) {
                    if (battle_unit_get_stats_from_battle_id(slot) == unit) {
                        break;
                    }
                }
                runtime_id = battle_unit_get_runtime_id_by_battle_id(slot);
                if (runtime_id == -1) {
                    unit_save_staged_status_data(slot);
                    battle_gfx_load_unit_graphics_by_battle_id(slot, flag);
                    g_unit_graphics_load_pending = 1;
                } else {
                    if (flag == 0) {
                        battle_unit_draw_and_activate_by_runtime_id(runtime_id);
                    }
                    if (unit_save_staged_status_data(slot) != 0) {
                        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                        thread_call_on_main_stack(runtime_id);
                    }
                }
            }
            while (1) {
                thread_yield();
                if (g_unit_graphics_load_pending == 0) {
                    break;
                }
                if (g_main_debug_display_enabled != 0) {
                    FntPrint(g_world_text_reading_character_message, slot);
                }
            }
        }
        slot++;
        script += g_script_event_instruction_sizes[script[0]] + 1;
    }
    thread_exit_current();
}
