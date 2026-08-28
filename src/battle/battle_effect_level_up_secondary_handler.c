#include "fft/battle.h"
#include "fft/battle_effect.h"

/*
 * Secondary effect function 0x0d (Level Up) in g_battle_effect_secondary_handlers.
 * It shares the venom trap
 * handler's shape but seeds group 5's position_y with -8 and lowers it by 3 per
 * executing frame instead of playing a sound. Returns 1 to keep animating.
 */
s32 battle_effect_level_up_secondary_handler(void) {
    battle_effect_secondary_data_t* effect;
    battle_effect_secondary_data_t* current;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    battle_effect_on_hit_vector_t pos;
    s32 result;
    s16 finish_timer;

    effect = g_battle_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch ((u32)effect->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        g_battle_effect_groups[5].position_y = -8;
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        battle_effect_start_group(5, 11);
        {
            /* Addressing through a pointer keeps the target lui/addiu pair. */
            s16* rise = &g_battle_effect_groups[5].position_y;
            *rise -= 3;
        }
        /* fallthrough */
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(0x230);
        pos.z = 0;
        pos.x = 0;
        pos.y = ONE;
        battle_effect_copy_on_hit_data_to_second_section();
        battle_effect_store_first_section_of_on_hit_data(&pos);
        battle_effect_update_active_slots(5);
        battle_effect_restore_inertia_threshold();
        battle_effect_copy_second_section_to_on_hit_data();
        current = g_battle_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0 && (s16)current->timer > (s32)g_battle_effect_groups[5].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}
