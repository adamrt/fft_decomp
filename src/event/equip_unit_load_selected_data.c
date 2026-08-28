#include "fft/equip.h"
#include "fft/world.h"
#include "psx/types.h"

/* Unprototyped view: the void definition is called here with (index, 1) in a0/a1. */
extern void equip_unit_copy_data_to_compare_slot();
void equip_unit_load_selected_data(void) {
    u8 unused[0x80];
    u8* unit;

    unit = (u8*)g_equip_unit_data[g_equip_unit_selected_index];
    if (*(s16*)(unit + 0x10) < *(s16*)(unit + 0xC)) {
        *(s16*)(unit + 0xC) = *(s16*)(unit + 0x10);
    }

    unit = (u8*)g_equip_unit_data[g_equip_unit_selected_index];
    if (*(s16*)(unit + 0x16) < *(s16*)(unit + 0x12)) {
        *(s16*)(unit + 0x12) = *(s16*)(unit + 0x16);
    }

    bcopy((u8*)g_equip_unit_data[g_equip_unit_selected_index], &g_equip_selected_unit_stat_summary, 0x22);
    bcopy((u8*)g_equip_unit_data[g_equip_unit_selected_index] + 0x22, g_equip_panel_selected_unit_data, 0xE);
    bcopy((u8*)g_equip_unit_data[g_equip_unit_selected_index] + 0x30, g_equip_unit_editor_stats, 0x40);
    equip_unit_calculate_equipment_stat_bonuses(
        g_equip_unit_editor_stats, g_equip_unit_data[g_equip_unit_selected_index]->equipment);
    equip_unit_copy_data_to_compare_slot(g_equip_unit_selected_index, 1);
    equip_menu_reset_state_flags();
}
