#include "fft/main_heap.h"
#include "fft/option.h"
#include "fft/status_panel.h"
#include "psx/types.h"

/* Reset the mutable fields and restore the embedded menu template. */
void option_gfx_init_scaled_draw_area_packets(status_panel_portrait_primitive_tail_t* record) {
    record->rects[0].w = 0;
    record->rects[1].h = 0;
    battle_copy_bytes(&record->rects[1], &g_option_gfx_draw_area_template, 8);
}
