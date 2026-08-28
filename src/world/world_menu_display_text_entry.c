#include "fft/world.h"
#include "psx/types.h"

/* Set the WORLD menu-text callback and dispatch the shared text lookup. */
void world_menu_display_text_entry(s32 p1, void* p2, void* p3) {
    g_world_thread_inner_subroutine_callback = (void (*)(void))world_menu_display_text;
    world_thread_call_on_main_stack(p1, p2, p3, 0);
}
