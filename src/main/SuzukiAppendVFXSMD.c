#include "fft/main_sound.h"
#include "psx/types.h"

/* Appends a loaded SED/SMD block to the end of the Suzuki driver's sound
 * resource list. */
void SuzukiAppendVFXSMD(main_sound_resource_t* resource) {
    main_sound_resource_t** slot;
    main_sound_resource_t* node;

    slot = &g_main_sound_resource_list;
    if (g_main_sound_resource_list != 0) {
        do {
            node = *slot;
            slot = &node->next;
        } while (node->next != 0);
    }
    *slot = resource;
    resource->next = 0;
}
