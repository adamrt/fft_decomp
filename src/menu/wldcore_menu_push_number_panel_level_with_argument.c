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

/* Pushes the number-panel menu level (type 0x30) that edits script variable
 * `variable_id`, starting the panel at `value`. */
void wldcore_menu_push_number_panel_level_with_argument(s32 variable_id, s32 value) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_wldcore_menu_ordering_table_offset = 2;
    g_wldcore_active_menu_value = value;
    origin.x = 0x20;
    origin.y = 0x20;
    wldcore_window_init_number_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0x270F0000, 0, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_records_next[record_index].panel.variable_id = variable_id;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_NUMBER_PANEL_WITH_ARGUMENT;
    g_wldcore_menu_stack_depth = record_index + 1;
}
