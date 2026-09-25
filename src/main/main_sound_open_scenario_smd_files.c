#include "fft/main_runtime.h"
#include "fft/main_sound.h"

int main_sound_open_scenario_smd_files(int scenario) {
    int checked_slots = 0;
    /* Byte-offset form kept: indexing g_main_sound_scenario_smd_files[scenario]
     * schedules the scale away from where the target computes it. */
    int file_offset = scenario * 8;
    int slot = 1;
    suzuki_smd_header_t** loaded_smd_base = &g_main_sound_music.slots.smd[1];
    /* Pinned: unpinned, GCC gives the la to the walking pointer and copies
     * it to the base ($s1/$s4 swapped); index forms bias the pointer by 4. */
    register suzuki_smd_header_t** loaded_smd __asm__("$17") = loaded_smd_base;
    suzuki_smd_header_t* buffer;

    do {
        checked_slots++;
        if (*loaded_smd == 0) {
            buffer = main_heap_alloc_smd(*(int*)((char*)g_main_sound_scenario_smd_files + file_offset + 4));
            if (buffer != 0) {
                if (main_file_call_build_header(*(int*)((char*)g_main_sound_scenario_smd_files + file_offset),
                        *(int*)((char*)g_main_sound_scenario_smd_files + file_offset + 4), buffer)
                    == 0) {
                    *loaded_smd = buffer;
                    /* slots.handles[0], addressed from the smd base: the member
                     * spelling stores through a fresh absolute address. */
                    *(s32*)((char*)loaded_smd_base - 0x20) = slot;
                } else {
                    main_system_handle_animation_exception(0x11);
                    return 0;
                }
            } else {
                main_system_handle_animation_exception(0x11);
                return 0;
            }
            return slot;
        }
        slot++;
        loaded_smd++;
    } while (checked_slots < 2);
    return 0;
}
