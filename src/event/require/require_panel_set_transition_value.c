#include "fft/event_require.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Set the three panel dim flags and both status animation flags to `value`. */
void require_panel_set_transition_value(s32 value) {
    g_require_panel_dim_a = value;
    g_require_panel_dim_b = value;
    g_require_panel_dim_c = value;
    g_menu_panel_status_animation[0] = value;
    g_menu_panel_status_animation[1] = value;
}
