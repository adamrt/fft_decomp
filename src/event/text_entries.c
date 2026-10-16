/* Stacked glyph pairs and the display of text entries and strings. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void menu_draw_stacked_glyph_pair(u8* pixels, glyph_blit_t* position) {
    position->rect.x -= 2;
    position->rect.y += 2;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_upper, position);
    position->rect.y += 4;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_lower, position);
}

/* Dispatch one text entry through the scene's main-stack renderer. */
void menu_display_text_entry(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(text_id, image, origin, 0);
}

/* Dispatch with the renderer's continue-past-choice-marks argument. */
void menu_display_text_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(text_id, image, origin, 1);
}

/* Dispatch a supplied text cursor through the scene's main-stack renderer. */
void menu_display_text_string(u8* image, glyph_blit_t* origin, u8* text) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(0, image, origin, text);
}
