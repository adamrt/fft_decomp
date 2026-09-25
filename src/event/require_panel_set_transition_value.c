#include "fft/event_require.h"
#include "psx/types.h"

void require_panel_set_transition_value(s32 value) {
    g_require_panel_dim_a = value;
    g_require_panel_dim_b = value;
    g_require_panel_dim_c = value;
    g_require_panel_status_animation[0] = value;
    g_require_panel_status_animation[1] = value;
}
