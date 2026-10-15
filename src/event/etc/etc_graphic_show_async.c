#include "fft/event_etc.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Thread body: load ETC graphic parameter 1 and run its show routine, the
 * chapter title for texture mode 0 or the game-over screen for mode 1. */
void etc_graphic_show_async(void) {
    s32 graphic_id;
    s32 graphic_type;

    graphic_id = thread_get_current_parameter_1_variant();
    etc_graphic_open(graphic_id);
    graphic_type = g_etc_graphics[graphic_id].texture_mode;
    if (graphic_type == 0) {
        etc_graphic_show_chapter_title(graphic_id);
    } else if (graphic_type == 1) {
        etc_graphic_show_game_over(graphic_id);
    }
    thread_exit_current();
}
