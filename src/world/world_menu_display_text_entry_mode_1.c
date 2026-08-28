#include "fft/world.h"
#include "psx/types.h"

void world_menu_display_text_entry_mode_1(s32 p1, s32 p2, s32 p3) {
    g_world_thread_inner_subroutine_callback = (void (*)(void))world_menu_display_text;
    world_thread_call_on_main_stack(p1, p2, p3, 1);
}
