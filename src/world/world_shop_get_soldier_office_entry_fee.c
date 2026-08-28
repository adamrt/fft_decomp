#include "fft/world.h"
#include "psx/types.h"

s32 world_shop_get_soldier_office_entry_fee(void) {
    s16 mode = g_world_shop_hire_menu_thread_data.cursor;

    if (mode == 0) {
        return 0x5DC;
    }
    return (mode == 1) ? 0x578 : 0;
}
