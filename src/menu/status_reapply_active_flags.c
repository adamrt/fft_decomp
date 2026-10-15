#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "psx/types.h"

void status_reapply_active_flags(unit_stats_t* unit) {
    s32 i;
    s32 battle_id = unit->battle_id;

    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_runtime_graphics_flag_change(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    mem_zero(unit->status_sets.innate, 0x1F);
    mem_zero(unit->inflicted_status, STATUS_BYTE_COUNT);
}
