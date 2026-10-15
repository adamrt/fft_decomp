#include "fft/battle_unit.h"

SVECTOR* battle_unit_get_event_offset_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit == 0) {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return &unit->event_offset;
}
