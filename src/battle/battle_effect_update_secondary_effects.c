#include "fft/battle.h"

typedef s32 (*battle_secondary_effect_function_t)(void);

extern battle_secondary_effect_function_t g_battle_effect_secondary_handlers[];

s32 battle_effect_update_secondary_effects(void) {
    battle_effect_secondary_data_t* effect;
    u32 previous_slot_id;

    g_battle_effect_polygon_depth_data = main_gfx_get_otag();
    g_battle_effect_buffer_index = 1 - g_battle_effect_buffer_index;
    g_battle_effect_current_slot_id = g_battle_effect_active_slot_tail;
    if (g_battle_effect_active_slot_tail == 0) {
        return 0;
    }

    do {
        effect = &g_battle_effect_secondary_data[g_battle_effect_current_slot_id];
        g_battle_effect_current_secondary = effect;
        previous_slot_id = effect->previous_slot_id;
        if (g_battle_effect_secondary_handlers[effect->function_id]() == 0) {
            battle_effect_remove_secondary(g_battle_effect_current_slot_id);
        }
        g_battle_effect_current_slot_id = previous_slot_id;
    } while (previous_slot_id != 0);

    return 1;
}
