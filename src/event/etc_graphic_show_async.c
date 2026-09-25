#include "fft/event_etc.h"
#include "psx/types.h"

void etc_graphic_show_async(void) {
    s32 graphic_id;
    s32 graphic_type;

    graphic_id = battle_thread_get_current_parameter_1();
    etc_graphic_open(graphic_id);
    graphic_type = g_etc_graphics[graphic_id].texture_mode;
    if (graphic_type == 0) {
        etc_graphic_show_chapter_title(graphic_id);
    } else if (graphic_type == 1) {
        etc_graphic_show_game_over(graphic_id);
    }
    battle_thread_exit_current();
}
