#include "fft/main_sound.h"
#include "fft/open.h"

/* Build the three-entry sound-type menu and select the saved option.
 *
 * The target addresses sound_mode_selection through the field-base alias
 * g_open_controller_flags, but reads the selected render-record index through
 * the complete controller record. Keeping those two views distinct preserves
 * the independently materialized bases in the target. The unaddressed local
 * storage accounts for the target's 0x30-byte frame.
 *
 * Separate lifetimes for the append result and selected record preserve the
 * target's volatile-register allocation. */
void open_menu_push_sound_type_controller(void) {
    s32 i;
    s32 y;
    s32 record;
    open_render_record_36_t* records;
    open_render_record_36_position_t* positions;
    open_controller_record_t* controllers;
    /* Pin required: unpinned, the controller index and the stream base swap
     * $a0 and $a1. */
    register s32 controller_index __asm__("$4");
    s32 sound_type;
    s32 selected_record;
    open_sound_menu_state_t* menu;

    i = 0;
    controllers = g_open_controller_stream_start;
    records = g_open_gfx_render_records_36;
    positions = (open_render_record_36_position_t*)&records[0].x;
    y = 0x1e;
    for (; i < 3; i++, y += 0xc) {
        record = open_gfx_append_render_record_36(
            g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
        ((open_sound_menu_state_t*)&controllers[g_open_current_controller_index])->render_records[i] = record;
        g_open_gfx_render_records_36[record].ot_layer = 4;
        g_open_gfx_render_records_36[record].anim_id = i + 7;
        records[record].frame_timer = 0;
        g_open_gfx_render_records_36[record].frame_index = 0;
        positions[record].y = y;
        positions[record].x = -0x14;
    }

    g_open_controller_flags[g_open_current_controller_index * 25 + 1] = 0;
    sound_type = main_sound_get_type();
    controller_index = g_open_current_controller_index;
    g_open_controller_flags[controller_index * 25] = sound_type;
    menu = (open_sound_menu_state_t*)&g_open_controller_stream_start[controller_index];
    sound_type <<= 2;
    /* The explicit byte offset preserves the target's index shift before the
     * menu-base addition; direct array indexing schedules those instructions
     * in the opposite order. */
    sound_type += (s32)menu;
    selected_record = *(s32*)sound_type;
    g_open_current_controller_index = controller_index + 1;
    g_open_gfx_render_records_36[selected_record].palette = 9;
    g_open_controller_handler_indices[controller_index] = 3;
}
