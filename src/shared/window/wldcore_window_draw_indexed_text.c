#include "fft/wldcore.h"

/* Skip -1, otherwise draw the history text in text section 17 (0x8800). */
void wldcore_window_draw_indexed_text(wldcore_menu_sound_novel_history_level_t* level, s32 index) {
    /* Named text-array access reverses the final address-add operands. */
    s32 text_id = ((s32*)level)[index + level->scroll];

    if (text_id != -1) {
        wldcore_window_draw_centered_text(level->render_index, text_id + TEXT_ID_WORLD_MESSAGE_BASE);
    }
}
