#include "fft/equip.h"
#include "psx/types.h"

s32 equip_menu_get_preview_hp_bonus_display(void) {
    s32 value = g_equip_selected_unit_hp_bonus;

    if (value == 0) {
        return 0x20000000;
    }
    return value;
}
