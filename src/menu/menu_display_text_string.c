#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Dispatch a supplied text cursor through the scene's main-stack renderer. */
void menu_display_text_string(u8* image, glyph_blit_t* origin, u8* text) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(0, image, origin, text);
}
