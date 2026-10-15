#ifndef TEXT_H
#define TEXT_H

/* Text rendering shared by BATTLE and WORLD: glyph images, message threads
 * and the message strings they print. */

#include "fft/gfx.h"
#include "psx/types.h"

/* messages */
extern char g_text_wait_for_allocation_message[];

/* per-module bodies: each module links its own copy under the shared name. */
void text_draw_large_number_glyphs_variant(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos);
void text_init_section_pointers_variant(void);
void text_run_dialog_thread_variant(void);
void text_run_spell_quote_thread_variant(void);

#endif
