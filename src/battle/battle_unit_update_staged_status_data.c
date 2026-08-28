#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_gfx.h"
#include "fft/thread.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

typedef struct {
    u8 unknown_000[0x5F0];
    unit_status_staging_t status_staging; /* 0x5f0 */
} battle_event_work_t;
typedef char battle_event_work_size_must_be_0x9e2[(sizeof(battle_event_work_t) == 0x9E2) ? 1 : -1];

/* Battle twin of world_unit_update_staged_status_data. Saves or restores the event status snapshot for every misc
 * record of a unit.
 *
 * With restore clear, each resolved battle unit is snapshotted and its graphics are
 * refreshed on the main stack; otherwise the snapshot is replayed through
 * battle_unit_apply_staged_status_data. Follow-up flags add 40-frame waits. */
void battle_unit_update_staged_status_data(u16 unit_id, u16 restore) {
    s32 misc_index;
    s32 unit_index;
    s32 battle_id;
    s32 battle_unit_index;

    g_battle_unit_status_staging_data = &((battle_event_work_t*)g_battle_ai_workspace_ptr)->status_staging;
    if (g_battle_menu_input_disabled == 0 && battle_unit_try_get_misc_data_by_unit_id(&unit_id, &misc_index) != 0) {
        unit_index = 0;
        g_battle_unit_status_staging_data->flags = 0;
        do {
            if (battle_script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &misc_index) != 0) {
                battle_unit_index = battle_unit_get_battle_index_by_misc_id(unit_id);
                if (battle_unit_index != -1) {
                    battle_find_unit_data_pointer_for_entd_unit_id(
                        battle_unit_get_stats_from_battle_id(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (restore == 0) {
                            if (battle_update_unit_status_and_staged_status_data(battle_unit_index) != 0) {
                                g_battle_thread_call_target
                                    = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                                battle_thread_call_on_main_stack(unit_id);
                            }
                        } else {
                            battle_unit_apply_staged_status_data(battle_unit_index, unit_id);
                        }
                    }
                }
                if (misc_index == 0) {
                    break;
                }
            }
            unit_index++;
        } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
        if (g_battle_unit_status_staging_data->flags != 0) {
            battle_thread_wait_frames(40);
        }
        if (g_battle_unit_status_staging_data->flags & 1) {
            battle_thread_wait_frames(40);
        }
        if (restore == 0) {
            g_battle_thread_call_target = battle_gfx_reset_jumping_unit_graphic_triggers;
            battle_thread_call_on_main_stack();
        }
    }
}
