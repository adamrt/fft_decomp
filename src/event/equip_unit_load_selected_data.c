#include "fft/event_equip.h"
#include "psx/types.h"

/* Unprototyped view: the void definition is called here with (index, 1) in a0/a1. */
extern void equip_unit_copy_data_to_compare_slot();
void equip_unit_load_selected_data(void) {
    u8 unused[0x80];
    equip_unit_data_t* unit;

    unit = g_equip_unit_data[g_equip_unit_selected_index];
    if (*(s16*)&unit->max_hp < *(s16*)&unit->hp) {
        *(s16*)&unit->hp = *(s16*)&unit->max_hp;
    }

    unit = g_equip_unit_data[g_equip_unit_selected_index];
    if (*(s16*)&unit->max_mp < *(s16*)&unit->mp) {
        *(s16*)&unit->mp = *(s16*)&unit->max_mp;
    }

    bcopy(g_equip_unit_data[g_equip_unit_selected_index], &g_equip_selected_unit_stat_summary, 0x22);
    bcopy(&g_equip_unit_data[g_equip_unit_selected_index]->entd_slot, g_equip_panel_selected_unit_data, 0xE);
    bcopy(&g_equip_unit_data[g_equip_unit_selected_index]->move, g_equip_unit_editor_stats, 0x40);
    equip_unit_calculate_equipment_stat_bonuses(
        g_equip_unit_editor_stats, g_equip_unit_data[g_equip_unit_selected_index]->equipment);
    equip_unit_copy_data_to_compare_slot(g_equip_unit_selected_index, 1);
    equip_menu_reset_state_flags();
}
