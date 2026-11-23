/* The ability list's "Now" label and main-stack text drawing. */

#include "fft/gfx/gfx.h"
#include "fft/menu/menu_gfx.h"
#include "fft/sys/thread.h"
#include "fft/text/text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static glyph_blit_t s_menu_charge_time_now_lower = { { 190, 28, 20, 4 }, 256, 0 };
static glyph_blit_t s_menu_charge_time_now_upper = { { 168, 28, 20, 4 }, 256, 0 };

/* Draw the ability list's "Now" charge time in two stacked halves. The caller
 * selects it for skill_ct 0 or 0xff; the pen ends on the lower half. */
void text_draw_charge_time_now(u8* pixels, glyph_blit_t* position) {
    position->rect.x -= 2;
    position->rect.y += 2;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &s_menu_charge_time_now_upper, position);
    position->rect.y += 4;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &s_menu_charge_time_now_lower, position);
}

/* Render text entry text_id into a 4bpp image at the pen origin, running text_render_to_image
 * on the main stack. */
void text_render_entry_to_image(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))text_render_to_image;
    thread_call_on_main_stack(text_id, image, origin, 0);
}

/* text_render_entry_to_image, with the renderer told to continue past choice marks. */
void text_render_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))text_render_to_image;
    thread_call_on_main_stack(text_id, image, origin, 1);
}

/* Render a text string the caller supplies instead of a text id, on the main stack. */
void text_render_string_to_image(u8* image, glyph_blit_t* origin, const u8* text) {
    g_thread_call_target = (void (*)(void))text_render_to_image;
    thread_call_on_main_stack(0, image, origin, text);
}
