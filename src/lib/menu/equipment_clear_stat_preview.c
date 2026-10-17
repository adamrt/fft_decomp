/* Clearing the HP and MP preview bonuses and the equipment detail stats. */

#include "fft/equipment.h"
#include "fft/menu.h"
#include "fft/unit_panel.h"

/* Clear the HP/MP preview bonuses and equipment-detail stats. */
void equipment_clear_stat_preview(unit_panel_summary_t* summary, unit_panel_record_t* detail) {
    summary->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    summary->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    detail->state.move = 0;
    detail->state.speed = 0;
    detail->state.jump = 0;
    detail->state.right_weapon_power = 0;
    detail->state.left_weapon_power = 0;
    detail->state.left_weapon_evade = 0;
    detail->state.right_weapon_evade = 0;
    detail->state.physical_attack = 0;
    detail->state.physical_class_evade = 0;
    detail->state.physical_shield_evade = 0;
    detail->state.physical_accessory_evade = 0;
    detail->state.magical_attack = 0;
    detail->state.magical_class_evade = 0;
    detail->state.magical_shield_evade = 0;
    detail->state.magical_accessory_evade = 0;
}
