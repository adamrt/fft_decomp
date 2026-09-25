#include "fft/event_helpmenu.h"
#include "psx/types.h"

void helpmenu_menu_close(void) {
    battle_copy_bytes(g_battle_text_section_pointers, g_helpmenu_text_saved_pointers, 0x80);
    main_heap_free(g_main_heap_high_overlay_load_address);
    g_battle_menu_help_open = 0;
    battle_thread_exit_current();
}
