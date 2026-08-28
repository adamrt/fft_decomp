#include "fft/battle.h"
#include "fft/main_heap.h"
#include "psx/types.h"

s32 battle_target_set_weapon_attack_panels(battle_ai_command_action_t* source) {
    u8 action[20];
    battle_stats_t* unit;
    s32 idx;
    s32 off;

    main_util_copy_action_data((const u8*)source, action);
    unit = &g_battle_unit_stats[action[0]];
    idx = unit->position.bits.y * g_map_max_x + unit->x;
    battle_target_clear_panel_data();
    battle_target_calculate_weapon_range(unit);
    {
        u8* p1;
        u8* p2;
        off = idx * 5;
        p1 = (u8*)g_battle_target_panel_data + off;
        p2 = (u8*)g_battle_target_panel_data + off + 0x500;
        *p1 = 0;
        *p2 = 0;
        /* The target passes p1 to the argument-less callee. */
        ((void (*)(u8*))battle_target_set_all_panels_targeted_if_targetable)(p1);
    }
}
