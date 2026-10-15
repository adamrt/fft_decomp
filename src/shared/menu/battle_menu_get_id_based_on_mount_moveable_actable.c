#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* The idle action menu for a unit id: battle_menu_get_unit_action_menu_id, with
 * the four TURN menus replaced by their RIDER_TURN variants for a rider. Returns
 * BATTLE_MENU_ID_NONE for an empty slot. */
battle_menu_id_e battle_menu_get_id_based_on_mount_moveable_actable(s32 battle_id) {
    unit_stats_t* unit;
    s32 menu;

    unit = battle_unit_get_existing_pointer(battle_id);
    if (unit == 0) {
        return BATTLE_MENU_ID_NONE;
    }
    menu = battle_menu_get_unit_action_menu_id(unit);
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) != 0) {
        switch (menu) {
        case BATTLE_MENU_ID_TURN:
            menu = BATTLE_MENU_ID_RIDER_TURN;
            break;
        case BATTLE_MENU_ID_TURN_MOVED:
            menu = BATTLE_MENU_ID_RIDER_TURN_MOVED;
            break;
        case BATTLE_MENU_ID_TURN_ACTED:
            menu = BATTLE_MENU_ID_RIDER_TURN_ACTED;
            break;
        case BATTLE_MENU_ID_TURN_MOVED_ACTED:
            menu = BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED;
            break;
        default:
            return menu;
        }
    }
    return menu;
}
