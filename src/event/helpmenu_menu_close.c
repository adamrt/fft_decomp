#include "fft/event_helpmenu.h"
#include "fft/memory.h"
#include "fft/thread.h"

/* Thread tail of the help menu: restore the 0x80 bytes of text section
 * pointers saved when it opened, free the overlay reservation and exit. */
void helpmenu_menu_close(void) {
    mem_copy_bytes_nonempty(g_text_section_pointers, g_helpmenu_text_saved_pointers, 0x80);
    main_heap_free(g_main_heap_high_overlay_load_address);
    g_menu_help_open = 0;
    thread_exit_current();
}
