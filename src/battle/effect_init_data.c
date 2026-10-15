#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/*
 * Effect-file load state machine: state 0 starts loading the ability's effect
 * file (falling back to effect 1 when its size is zero) 0x3500 bytes into the
 * event overlay area, state 1 polls the load, state 2 waits out the countdown
 * (60 frames when a secondary effect is active), finalizes the secondary
 * effect and fades the music when the effect header flag 0x10 is set, and
 * state 3 waits for the secondary effect phase to end. Returns nonzero while
 * work remains. A load state outside 0-3 leaves `result` unset: the test then
 * reads whatever the caller left in $a0.
 */
s32 effect_init_data(void) {
    s32 result;
    s32 lba;
    s32 byte_length;
    u8* destination;

    destination = g_main_heap_companion_overlay_load_address + 0x3500;
    if (g_battle_ability_effect_id >= 0) {
        switch (g_effect_load_state) {
        case EFFECT_LOAD_START:
            if (g_effect_charge_secondary_id != 0) {
                g_effect_load_countdown = 60;
            } else {
                g_effect_load_countdown = 0;
            }
            effect_load_lba_and_size(g_battle_ability_effect_id, &lba, &byte_length);
            if (byte_length == 0) {
                g_battle_ability_effect_id = EFFECT_FILE_CURE;
                effect_load_lba_and_size(EFFECT_FILE_CURE, &lba, &byte_length);
            }
            result = main_file_call_build_header(lba, byte_length, destination);
            if (result == 0) {
                g_effect_load_state = EFFECT_LOAD_READING;
                result = 1;
            }
            break;
        case EFFECT_LOAD_READING:
            result = main_file_is_still_loading();
            if (result == 0) {
                g_effect_load_state = EFFECT_LOAD_COUNTDOWN;
                result = 1;
            }
            break;
        case EFFECT_LOAD_COUNTDOWN:
            if (g_effect_load_countdown != 0) {
                result = 1;
                break;
            }
            if (g_effect_charge_secondary_id != 0) {
                effect_finalize_secondary(g_effect_charge_secondary_id);
            }
            g_effect_load_state = EFFECT_LOAD_ENDING_SECONDARY;
            effect_init_flags_section_pointer(g_battle_ability_effect_id);
            /* An `lw` of the section's leading 32-bit value, not a byte test. */
            if (g_effect_flags_section->flags & EFFECT_FLAG_QUIET_MUSIC) {
                main_sound_set_current_music_volume(0x40, 0xF0);
            }
            result = 1;
            break;
        case EFFECT_LOAD_ENDING_SECONDARY:
            result = effect_get_secondary_phase(g_effect_charge_secondary_id) != EFFECT_SECONDARY_PHASE_ENDED;
            break;
        }
        if (g_effect_load_countdown > 0) {
            g_effect_load_countdown--;
        }
        return result != 0;
    }
    return 0;
}
