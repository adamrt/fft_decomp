#include "fft/event_equip.h"
#include "fft/menu.h"

enum {
    EQUIP_SLOT_MARKER_WINDOW_STATE = 0,
    EQUIP_SLOT_MARKER_FIRST_OTAG = 14,
    EQUIP_SLOT_MARKER_SECOND_OTAG = 15,
};

/*
 * Draw the two-part marker for the selected equipment slot.
 *
 * `width` must stay a variable: written as the literal `0x40 - (slot == 0)`,
 * fold turns the subtraction into `slot ? 0x40 : 0x3f` and the compiler emits a
 * branch instead of the target's `li`/`sltiu`/`subu`. Assigning it before the
 * window call also keeps the constant load after the marker address arithmetic.
 *
 * Structurally identical to world_menu_draw_row_window_and_cursor, which draws
 * the same window/marker pair for the world-map menu.
 */
void equip_menu_draw_equipment_slot_marker(s32 unused, s32 slot, s32 override) {
    s16 x;
    s16 width;
    oriented_quad_t* marker;
    oriented_quad_t* secondary;
    s32 secondary_x;

    x = g_equip_menu_slot_marker_x[slot];
    width = 0x40;
    menu_run_script_with_palette_mode_variant(
        &g_equip_menu_slot_marker_window, EQUIP_SLOT_MARKER_WINDOW_STATE, override);
    marker = &g_equip_menu_slot_marker_rects[slot];
    marker->position.x = x;
    marker->position.y = width - (slot == 0);
    menu_gfx_enqueue_oriented_textured_quad(
        marker, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, EQUIP_SLOT_MARKER_FIRST_OTAG);
    secondary = &g_equip_menu_secondary_slot_marker;
    secondary_x = x + 9;
    if (slot == 3) {
        secondary_x = x + 7;
    }
    secondary->position.x = secondary_x;
    g_equip_menu_secondary_slot_marker.position.y = width + 10;
    menu_gfx_enqueue_oriented_textured_quad(secondary, 0, TEXTURE_UV_FLIP_NONE, 0, EQUIP_SLOT_MARKER_SECOND_OTAG);
}
