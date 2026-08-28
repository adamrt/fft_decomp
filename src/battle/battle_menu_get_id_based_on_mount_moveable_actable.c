#include "fft/battle.h"
#include "psx/types.h"

s32 battle_menu_get_id_based_on_mount_moveable_actable(s32 unit_id) {
    battle_stats_t* unit;
    s32 menu;

    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return -1;
    }
    menu = battle_menu_get_unit_action_menu_id(unit);
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) != 0) {
        switch (menu) {
        case 0x0:
            menu = 0x2e;
            break;
        case 0xe:
            menu = 0x2f;
            break;
        case 0x13:
            menu = 0x30;
            break;
        case 0x14:
            menu = 0x31;
            break;
        default:
            return menu;
        }
    }
    return menu;
}
