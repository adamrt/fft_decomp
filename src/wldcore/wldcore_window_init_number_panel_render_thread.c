#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Initialize and start a numeric panel renderer.
 *
 * The packed word supplies two displayed halfwords; their magnitude selects
 * a one- through four-digit panel width before the optional label is measured. */
void wldcore_window_init_number_panel_render_thread(
    s32 thread_id, u32 packed_values, s32 text_id, wldcore_point32_t* origin) {
    s16 text_width;
    s16 text_height;
    s32 digit_width;
    /* Pins required: unpinned, the low-half mask leaves the prologue and the
     * digit-count compare and origin load take different registers. */
    register s32 lower_value __asm__("$3") = packed_values & 0xffff;
    register s32 upper_value __asm__("$5") = packed_values >> 16;
    register s32 comparison __asm__("$2");
    s32 state_flags;
    s32 panel_height;
    s32 measured_width;
    s32 width_multiple;
    s32 origin_x;
    register s32 origin_y __asm__("$3");
    void (*thread_function)(void);
    world_menu_entry_t* state;

    g_wldcore_window_panel_number_state.unknown_04[0] = -1;
    g_wldcore_window_panel_number_state.unknown_04[1] = -1;
    g_wldcore_window_panel_number_state.on_change = wldcore_noop_8008ffd8;
    g_wldcore_window_panel_number_state.min = lower_value;
    g_wldcore_window_panel_number_state.max = upper_value;
    g_wldcore_text_bounds[1] = 0x7d0;
    g_wldcore_text_bounds[0] = 0x7d0;
    if (lower_value < 10 && upper_value < 10) {
        digit_width = 1;
    } else {
        digit_width = 4;
        if (upper_value < 1000) {
            digit_width = 3;
        }
        comparison = upper_value < 100;
        if (comparison != 0) {
            digit_width = 2;
        }
    }
    text_width = 0;
    if (text_id != 0) {
        world_text_measure_pixels(&text_width, &text_height, world_text_find_entry(text_id));
    }
    state = &g_wldcore_window_panel_render_state;
    /* Keeps the 0x200 constant after the state address setup. */
    __asm__("" : "=r"(state) : "0"(state));
    state_flags = 0x200;
    state->vram_x = state_flags;
    state_flags = 0x100;
    panel_height = 0x20;
    g_wldcore_window_panel_render_state.vram_y = state_flags;
    /* Keeps the vram_y store ahead of the digit-width multiply. */
    __asm__ volatile("" : "=r"(digit_width) : "0"(digit_width) : "memory");
    width_multiple = digit_width * 6;
    /* Keeps the text-width load after the multiply. */
    __asm__("" : "=r"(width_multiple) : "0"(width_multiple));
    measured_width = text_width;
    g_wldcore_window_panel_render_state.inner_height = panel_height;
    measured_width += 0x18;
    width_multiple += measured_width;
    digit_width = width_multiple & 0xfffc;
    g_wldcore_window_panel_render_state.inner_width = digit_width;
    origin_x = origin->x;
    /* Keeps the width sum above in the target's $v0/$v1 roles. */
    __asm__ volatile("" : "=r"(origin_x) : "0"(origin_x) : "memory");
    thread_function = world_menu_labeled_number_entry_thread;
    g_wldcore_window_panel_render_state.window_x = origin_x;
    origin_y = origin->y;
    g_wldcore_window_panel_render_state.parent_indices = g_wldcore_text_bounds;
    g_wldcore_window_panel_render_state.text_binding
        = (struct world_menu_text_binding*)&g_wldcore_window_panel_number_state; /* a value range */
    g_wldcore_window_panel_render_state.window_width = digit_width;
    g_wldcore_window_panel_render_state.window_height = panel_height;
    g_wldcore_window_panel_render_state.unknown_10[0] = 0;
    g_wldcore_window_panel_render_state.unknown_10[1] = 0;
    g_wldcore_window_panel_render_state.overall_width = digit_width;
    g_wldcore_window_panel_render_state.overall_height = panel_height;
    g_wldcore_window_panel_render_state.unknown_18[0] = 0;
    g_wldcore_window_panel_render_state.unknown_18[1] = 0;
    g_wldcore_window_panel_render_state.text_id = text_id;
    g_wldcore_window_panel_render_state.max_row_index = 0;
    g_wldcore_window_panel_render_state.field_0x20 = 0;
    g_wldcore_window_panel_render_state.header_id = 0;
    g_wldcore_window_panel_render_state.value = &g_wldcore_active_menu_value;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_window_panel_render_state.select_text_table = 0;
    g_wldcore_window_panel_render_state.window_y = origin_y + 8;
    world_thread_start(thread_id, thread_function);
    world_thread_set_parameters(thread_id, (s32)state, 0, 0);
}
