#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_script_teleportout_event_instruction(s32 unit_id, s32 remove) {
    s32 misc;
    s32 idx;

    misc = battle_get_misc_id(unit_id);
    if (misc == 0x7D0) {
        return;
    }
    g_sound_effect_id_to_play = 0x6A;
    g_battle_thread_call_target = (void (*)(void))battle_effect_set_secondary_teleport_by_misc_id;
    battle_thread_call_on_main_stack(misc);
    idx = battle_unit_get_battle_index_by_misc_id(misc);
    battle_unit_set_specific_animation_value_on_battle_init(misc, 0x1B);
    battle_gfx_init_misc_unit_palette_modulation(misc, 2, 0x1F, 0x1F, 0x1F);
    battle_thread_wait_frames(0x3C);
    battle_gfx_init_misc_unit_palette_modulation(misc, 2, -0x1F, -0x1F, -0x1F);
    battle_thread_wait_frames(0x3C);
    if (remove != 0) {
        battle_unit_set_cannot_exist(idx);
    }
}
