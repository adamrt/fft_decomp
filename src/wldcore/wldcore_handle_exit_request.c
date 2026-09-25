#include "fft/wldcore.h"
#include "psx/types.h"

/* Called at the top of wldcore_entrypoint's loop; a nonzero script
 * `EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT` clears system flag 0x10000 and
 * makes the entrypoint return. */
s32 wldcore_handle_exit_request(void) {
    if (world_script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0) {
        g_main_system_flags &= ~0x10000;
        world_formation_restore_menu_after_core_exit();
        return 1;
    }

    return 0;
}
