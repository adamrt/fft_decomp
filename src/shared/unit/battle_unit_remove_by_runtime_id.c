#include "fft/battle_unit.h"

/* battle_unit_remove by runtime id. Returns 1, or 0 after raising pointer
 * exception 12 when no record has the id. */
s32 battle_unit_remove_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xFFFF);
    if (unit != 0) {
        battle_unit_remove(unit);
        return 1;
    }
    main_system_handle_pointer_exception(0xC);
    return 0;
}
