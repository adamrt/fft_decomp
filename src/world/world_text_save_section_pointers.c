#include "fft/world.h"
#include "psx/types.h"

/* Back up the text section pointer table once; world_text_restore_section_pointers undoes it. */
void world_text_save_section_pointers(void) {
    if (g_world_text_section_pointers_saved == 0) {
        world_script_copy_bytes(
            g_world_text_saved_section_pointers, g_world_text_section_pointers, sizeof(g_world_text_section_pointers));
        g_world_text_section_pointers_saved += 1;
    }
}
