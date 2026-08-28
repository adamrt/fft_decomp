#include "fft/battle.h"
#include "fft/battle_effect.h"

/*
 * Secondary effect function 0x15 (Equip Break) in g_battle_effect_secondary_handlers.
 * Same shape as the venom trap
 * handler, but effect group 11 is held in a variable, so its spawn_start_frame lookup
 * is an indexed table read. Returns 1 to keep animating.
 */
s32 battle_effect_equip_break_secondary_handler(void) {
    battle_effect_secondary_data_t* effect;
    battle_effect_secondary_data_t* current;
    battle_effect_on_hit_vector_t pos;
    s32 group;
    s32 result;
    s16 finish_timer;

    group = 11;
    effect = g_battle_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch ((u32)effect->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        result = 1;
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        battle_effect_start_group(group, 10);
        /* fallthrough */
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(0x230);
        pos.z = 0;
        pos.x = 0;
        pos.y = ONE;
        battle_effect_copy_on_hit_data_to_second_section();
        battle_effect_store_first_section_of_on_hit_data(&pos);
        battle_effect_update_active_slots(group);
        battle_effect_restore_inertia_threshold();
        battle_effect_copy_second_section_to_on_hit_data();
        current = g_battle_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0 && (s16)current->timer > (s32)g_battle_effect_groups[group].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}
