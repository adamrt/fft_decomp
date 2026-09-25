#include "fft/world.h"
#include "psx/types.h"

void world_text_restore_sections_and_stop_thread(void) {
    world_script_copy_bytes(
        g_world_text_section_pointers, g_world_text_section_pointer_backup, sizeof(g_world_text_section_pointers));
    main_heap_free(g_main_heap_high_overlay_load_address);
    g_world_text_overlay_active = 0;
    world_thread_exit_current();
}
