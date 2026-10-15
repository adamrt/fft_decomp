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

/* Renders up to 10 script-variable rows in the debug list panel.
 *
 * `value` first holds the row count for the panel height and then each
 * variable's value: one register (v1) in the target. */
void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level) {
    RECT bounds;
    point32_t point;
    s32 index;
    point32_t* base;
    point32_t* records;
    s32 variable;
    s32 text_id;
    u32 value;
    s32 i;

    variable = level->first_variable;
    if (variable + 10 >= 1024) {
        level->row_count = 1024 - variable;
    } else {
        level->row_count = 10;
    }
    value = level->row_count;
    index = level->render_index;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x68;
    bounds.h = value * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    /* Direct record indexing moves the symbol load after the index scaling
     * and breaks the exact match. Retain the target's biased base and stride. */
    records = &g_wldcore_window_render_records[0].base;
    base = (point32_t*)((u8*)records + index * sizeof(wldcore_window_render_record_t));
    base->x = -(bounds.w / 2);
    base->y = -88;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->row_count; i++) {
        value = script_get_variable(variable);
        if (value < 10000) {
            g_text_substitution_values[0] = variable;
            g_text_substitution_values[1] = value;
            text_id = 0x24;
        } else {
            g_text_substitution_values[0] = variable;
            text_id = 0x25;
        }
        window_display_text_entry_variant(
            index, text_id | TEXT_ID_WORLD_MENU_BASE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        variable++;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}
