#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/main_sound.h"

/*
 * Secondary effect function 0x06 (Red Orbs) in g_battle_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 8, then step the effect slots with a
 * (0, 0x1000, 0) on-hit vector until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 battle_effect_red_orbs_secondary_handler(void) {
    battle_effect_secondary_data_t* effect;
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
        SuzukiPlaySoundFindChannel(8);
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        battle_effect_start_group(10, 11);
        /* fallthrough */
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(0x230);
        pos.z = 0;
        pos.x = 0;
        pos.y = ONE;
        battle_effect_copy_on_hit_data_to_second_section();
        battle_effect_store_first_section_of_on_hit_data(&pos);
        battle_effect_update_active_slots(10);
        battle_effect_restore_inertia_threshold();
        battle_effect_copy_second_section_to_on_hit_data();
        effect = g_battle_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0 && (s16)effect->timer > (s32)g_battle_effect_groups[10].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}
