#include "fft/main.h"
#include "psx/types.h"

/* Unlinks resource from the loaded VFX sound list after stopping the SFX
 * channels that play its sounds.
 *
 * The search and unlink use a copy of the parameter while the call passes the
 * parameter itself, which stays in $a0 (the target loads no argument). The
 * copy sched2 schedules for it also keeps the list-head load after the
 * prologue saves; reorg then deletes that copy as redundant. */
void main_sound_remove_vfx_resource(main_sound_resource_t* resource) {
    main_sound_resource_t* prev;
    main_sound_resource_t* node;
    main_sound_resource_t* removed;

    removed = resource;
    prev = 0;
    node = g_main_sound_resource_list;
    while (node != 0) {
        if (node == removed) {
            break;
        }
        prev = node;
        node = node->next;
    }
    if (node != 0) {
        main_sound_stop_resource_sfx(resource);
        if (prev != 0) {
            prev->next = removed->next;
        } else {
            g_main_sound_resource_list = removed->next;
        }
    }
}
