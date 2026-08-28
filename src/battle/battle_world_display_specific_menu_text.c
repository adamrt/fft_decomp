#include "fft/battle_text.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Render text into a menu image at origin through the main-stack text
 * renderer. */
void battle_world_display_specific_menu_text(s32 image, s32 origin, s32 text) {
    g_battle_thread_call_target = (void (*)(void))battle_menu_display_text;
    battle_thread_call_on_main_stack(0, image, origin, text);
}
