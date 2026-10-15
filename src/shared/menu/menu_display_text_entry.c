#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Dispatch one text entry through the scene's main-stack renderer. */
void menu_display_text_entry(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(text_id, image, origin, 0);
}
