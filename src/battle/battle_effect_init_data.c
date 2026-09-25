#include "fft/battle.h"
#include "psx/types.h"

/*
 * Effect-file load state machine: state 0 starts loading the ability's effect
 * file (falling back to effect 1 when its size is zero) 0x3500 bytes into the
 * event overlay area, state 1 polls the load, state 2 waits out the countdown
 * (60 frames when a secondary effect is active), finalizes the secondary
 * effect and fades the music when the effect header flag 0x10 is set, and
 * state 3 waits for the secondary effect phase to end. Returns nonzero while
 * work remains. The unused parameter is the result register: unhandled states
 * return whatever the caller left in $a0.
 */
s32 battle_effect_init_data(s32 result) {
    s32 lba;
    s32 byte_length;
    u8* destination;

    destination = g_event_overlay_load_address + 0x3500;
    if (g_ability_effect_id >= 0) {
        switch (g_effect_load_state) {
        case 0:
            if (g_battle_effect_target_id != 0) {
                g_battle_effect_load_countdown = 60;
            } else {
                g_battle_effect_load_countdown = 0;
            }
            battle_effect_load_lba_and_size(g_ability_effect_id, &lba, &byte_length);
            if (byte_length == 0) {
                g_ability_effect_id = 1;
                battle_effect_load_lba_and_size(1, &lba, &byte_length);
            }
            result = main_file_call_build_header(lba, byte_length, destination);
            if (result == 0) {
                g_effect_load_state = 1;
                result = 1;
            }
            break;
        case 1:
            result = main_file_is_still_loading();
            if (result == 0) {
                g_effect_load_state = 2;
                result = 1;
            }
            break;
        case 2:
            if (g_battle_effect_load_countdown != 0) {
                result = 1;
                break;
            }
            if (g_battle_effect_target_id != 0) {
                battle_effect_finalize_secondary(g_battle_effect_target_id);
            }
            g_effect_load_state = 3;
            battle_effect_init_flags_section_pointer(g_ability_effect_id);
            /* An `lw` of the section's leading word, not a byte test. */
            if (g_battle_effect_flags_section->flags & EFFECT_FLAG_AUDIO_FADE) {
                main_sound_set_current_music_volume(0x40, 0xF0);
            }
            result = 1;
            break;
        case 3:
            result = battle_effect_get_secondary_phase(g_battle_effect_target_id) != 0;
            break;
        }
        if (g_battle_effect_load_countdown > 0) {
            g_battle_effect_load_countdown--;
        }
        return result != 0;
    }
    return 0;
}
