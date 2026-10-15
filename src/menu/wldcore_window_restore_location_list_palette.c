#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Restore the location list's optional renderer palette. */
void wldcore_window_restore_location_list_palette(wldcore_menu_stack_record_t* level) {
    if (level->location_list.extra_render != -1) {
        g_wldcore_window_render_records[level->location_list.extra_render].palette = 0;
    }
}
