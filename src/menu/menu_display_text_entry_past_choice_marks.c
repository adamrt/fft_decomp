#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Dispatch with the renderer's continue-past-choice-marks argument. */
void menu_display_text_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(text_id, image, origin, 1);
}
