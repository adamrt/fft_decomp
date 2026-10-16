#include "psx/suzuki.h"

/* Unlinks resource from the loaded VFX sound list after stopping the SFX
 * channels that play its sounds.
 *
 * The search and unlink use a copy of the parameter while the call passes the
 * parameter itself, which stays in $a0 (the target loads no argument). The
 * copy sched2 schedules for it also keeps the list-head load after the
 * prologue saves; reorg then deletes that copy as redundant. */
void suzuki_sound_remove_resource(suzuki_sound_resource_t* resource) {
    suzuki_sound_resource_t* prev;
    suzuki_sound_resource_t* node;
    suzuki_sound_resource_t* removed = resource;

    prev = 0;
    node = g_suzuki_sound_resource_list;
    while (node != 0) {
        if (node == removed) {
            break;
        }
        prev = node;
        node = node->next;
    }
    if (node != 0) {
        suzuki_sound_stop_resource_sfx(resource);
        if (prev != 0) {
            prev->next = removed->next;
        } else {
            g_suzuki_sound_resource_list = removed->next;
        }
    }
}
