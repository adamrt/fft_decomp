#include "fft/event_debugchr.h"
#include "psx/types.h"

void debugchr_editor_apply_values_to_battle_unit(s32 unit_id) {
    battle_stats_t* unit;
    s32 zodiac;
    u32 unit_flags;

    unit = battle_unit_get_stats_from_battle_id(unit_id);

    if (g_debugchr_editor_first_maximum < g_debugchr_editor_values[0])
        g_debugchr_editor_values[0] = g_debugchr_editor_first_maximum;
    if (g_debugchr_editor_second_maximum < g_debugchr_editor_values[3])
        g_debugchr_editor_values[3] = g_debugchr_editor_second_maximum;
    if (g_debugchr_editor_third_maximum < g_debugchr_editor_values[6])
        g_debugchr_editor_values[6] = g_debugchr_editor_third_maximum;
    unit->hp = g_debugchr_editor_values[0];
    unit->mp = g_debugchr_editor_values[3];
    unit->ct = g_debugchr_editor_values[6];
    unit->level = g_debugchr_panel_selected_billboard.level;
    unit->experience = g_debugchr_panel_selected_billboard.experience;
    /* Halfword objects (the loader stores sh); this function reads only the
     * low byte, and the lvalue form emits the target's bare lbu. */
    unit->brave = *(u8*)&g_debugchr_editor_brave;
    unit->faith = *(u8*)&g_debugchr_editor_faith;
    /* Zodiac is the top nibble of the birthday halfword. Union-member access
     * instead of the u16 view reorders the stores. */
    zodiac = g_debugchr_editor_zodiac << 12;
    unit_flags = *(u16*)&unit->birthday & 0x0fff;
    *(u16*)&unit->birthday = unit_flags | zodiac;
    unit->move = g_debugchr_editor_move;
    unit->attributes[UNIT_ATTRIBUTE_SPEED] = g_debugchr_editor_speed;
    unit->jump = g_debugchr_editor_jump;
}
