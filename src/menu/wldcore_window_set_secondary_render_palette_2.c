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

/* Sets the second owned render record's palette selector when its slot is present. */
void wldcore_window_set_secondary_render_palette_2(wldcore_menu_slot_set_t* slots) {
    if (slots->render_b != -1) {
        g_wldcore_window_render_records[slots->render_b].palette = 2;
    }
}
