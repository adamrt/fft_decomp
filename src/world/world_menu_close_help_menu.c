#include "fft/memory.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Thread tail of the help menu: restore the text section pointers saved
 * when it opened, free the overlay reservation and exit. */
void world_menu_close_help_menu(void) {
    mem_copy_bytes_nonempty(
        g_text_section_pointers, g_world_text_section_pointer_backup, sizeof(g_text_section_pointers));
    main_heap_free(g_main_heap_high_overlay_load_address);
    g_menu_help_open = 0;
    thread_exit_current();
}
