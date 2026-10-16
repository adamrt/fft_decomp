/* Stacked glyph pairs and the display of text entries and strings. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw g_menu_stacked_glyph_upper and, four rows lower, g_menu_stacked_glyph_lower, starting two
 * pixels left of and below the pen. The battle ability lists draw it in the number column instead
 * of a value of 0 or 1 when the ability's skill_ct is 0 or 0xff. */
void menu_draw_stacked_glyph_pair(u8* pixels, glyph_blit_t* position) {
    position->rect.x -= 2;
    position->rect.y += 2;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_upper, position);
    position->rect.y += 4;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_lower, position);
}

/* Render text entry text_id into a 4bpp image at the pen origin, running menu_display_text
 * on the main stack. */
void menu_display_text_entry(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text;
    thread_call_on_main_stack(text_id, image, origin, 0);
}

/* menu_display_text_entry, with the renderer told to continue past choice marks. */
void menu_display_text_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text;
    thread_call_on_main_stack(text_id, image, origin, 1);
}

/* Render a text string the caller supplies instead of a text id, on the main stack. */
void menu_display_text_string(u8* image, glyph_blit_t* origin, u8* text) {
    g_thread_call_target = (void (*)(void))menu_display_text;
    thread_call_on_main_stack(0, image, origin, text);
}
