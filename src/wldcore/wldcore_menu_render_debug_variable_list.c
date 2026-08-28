#include "fft/wldcore.h"
#include "fft/world.h"

/* Renders up to 10 script-variable rows in the debug list panel.
 *
 * The row-count and bounds constraints preserve the target's separate count
 * register and store order while preparing the aggregate call arguments. */
void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level) {
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t point;
    s32 index;
    wldcore_point32_t* base;
    wldcore_point32_t* records;
    s32 variable;
    s32 text_id;
    u32 value;
    s32 i;
    s32 count;
    /* Pin: places the index argument copy (`move a0,s2`) ahead of the dimensions.y computation. */
    register s32 call_index __asm__("$4");
    s32 zero;

    variable = level->first_variable;
    if (variable + 10 >= 1024) {
        level->row_count = 1024 - variable;
    } else {
        level->row_count = 10;
    }
    zero = 0;
    /* Materialises the 0 argument (`move a3,zero`) here, right after the row_count store. */
    __asm__("" : "=r"(zero) : "0"(zero));
    count = level->row_count;
    index = level->render_index;
    bounds.position.x = 0;
    bounds.position.y = 0;
    bounds.dimensions.x = 0x68;
    bounds.dimensions.y = count * 0x10 + 0x10;
    call_index = index;
    /* Keeps the bounds stores in target order, ahead of the image-buffer address setup. */
    __asm__ volatile("" : : "r"(count), "m"(bounds), "r"(call_index) : "memory");
    wldcore_window_build_render_record_image(
        call_index, bounds.position, bounds.dimensions, zero, g_wldcore_window_image_buffer);
    /* Direct record indexing moves the symbol load after the index scaling
     * and breaks the exact match. Retain the target's biased base and stride. */
    records = (wldcore_point32_t*)&g_wldcore_window_render_records[0].base_x;
    base = (wldcore_point32_t*)((u8*)records + index * sizeof(wldcore_window_render_record_t));
    base->x = -((s16)bounds.dimensions.x / 2);
    base->y = -88;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->row_count; i++) {
        value = world_script_get_variable(variable);
        if (value < 10000) {
            g_world_text_substitution_values[0] = variable;
            g_world_text_substitution_values[1] = value;
            text_id = 0x24;
        } else {
            g_world_text_substitution_values[0] = variable;
            text_id = 0x25;
        }
        wldcore_menu_display_text_entry(index, text_id | 0xB800, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        variable++;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}
