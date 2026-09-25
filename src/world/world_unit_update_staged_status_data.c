#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Saves or restores the event status snapshot for every misc record of a unit.
 *
 * With restore clear, each resolved battle unit is snapshotted and its graphics are
 * refreshed on the main stack; otherwise the snapshot is replayed through
 * world_unit_apply_staged_status. Follow-up flags add 40-frame waits. */
void world_unit_update_staged_status_data(u16 unit_id, u16 restore) {
    s32 misc_index;
    s32 unit_index;
    s32 battle_id;
    s32 battle_unit_index;

    g_world_unit_status_staging_data = &((world_event_work_t*)g_battle_ai_workspace_ptr)->status_staging;
    if (g_world_menu_input_disabled == 0 && world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        unit_index = 0;
        g_world_unit_status_staging_data->flags = 0;
        do {
            if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &misc_index) != 0) {
                battle_unit_index = battle_unit_get_battle_index_by_misc_id(unit_id);
                if (battle_unit_index != -1) {
                    battle_find_unit_data_pointer_for_entd_unit_id(
                        battle_unit_get_stats_from_battle_id(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (restore == 0) {
                            if (world_update_unit_status_and_staged_status_data(battle_unit_index) != 0) {
                                g_world_thread_call_target
                                    = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                                world_thread_call_on_main_stack(unit_id);
                            }
                        } else {
                            world_unit_apply_staged_status(battle_unit_index, unit_id);
                        }
                    }
                }
                if (misc_index == 0) {
                    break;
                }
            }
            unit_index++;
        } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
        if (g_world_unit_status_staging_data->flags != 0) {
            world_thread_wait_frames(40);
        }
        if (g_world_unit_status_staging_data->flags & 1) {
            world_thread_wait_frames(40);
        }
        if (restore == 0) {
            g_world_thread_call_target = battle_gfx_reset_jumping_unit_graphic_triggers;
            world_thread_call_on_main_stack();
        }
    }
}
