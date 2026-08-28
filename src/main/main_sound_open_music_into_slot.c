#include "fft/main_sound.h"
#include "psx/types.h"

/* Records the slot that was just opened (handles[0], 0x8004d970). The
 * do/while(0) wrapper is load-bearing: the retail allocation (slot in s2,
 * the slot pointer in s3) needs this use of `slot` weighted as loop-nested,
 * which is what a statement macro of this shape produces. */
#define set_opened_music_slot(slot)                                                                                    \
    do {                                                                                                               \
        g_main_sound_music.slots.handles[0] = (slot);                                                                  \
    } while (0)

/* Loads scenario music file `scenario` into music slot `slot` (1-based) if
 * the slot is free. Returns the slot, or 0 when it was occupied or the load
 * failed (which also raises exception 0x11). */
s32 main_sound_open_music_into_slot(s32 scenario, s32 slot) {
    if (g_main_sound_music.slots.smd[slot] == 0) {
        void* data = main_heap_alloc_smd(g_main_sound_scenario_smd_files[scenario].size);

        if (data != 0
            && main_file_call_build_header(g_main_sound_scenario_smd_files[scenario].sector,
                   g_main_sound_scenario_smd_files[scenario].size, data)
                == 0) {
            g_main_sound_music.slots.smd[slot] = data;
            set_opened_music_slot(slot);
            return slot;
        }
        main_system_handle_animation_exception(0x11);
        return 0;
    }
    return 0;
}
