#include "fft/battle_gfx.h"

/* battle_gfx_prepare_post_action_display for a runtime id. An absent unit raises
 * pointer exception 0xc and returns 0. */
s32 battle_gfx_prepare_post_action_display_by_runtime_id(s32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        battle_gfx_prepare_post_action_display(unit);
        return 1;
    }
    main_system_handle_pointer_exception(0xc);
    return 0;
}
