#include "fft/main_runtime.h"
#include "fft/open.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Title-menu controller state (handler 2), pushed into the next controller record. */
typedef struct open_title_menu_state {
    /* 0x00 */ s32 records_36[5]; /* logo, then the four menu items */
    /* 0x14 */ s32 idle_timer;
    /* 0x18 */ s32 cd_end_position;
    /* 0x1c */ s32 selected_item;
    /* 0x20 */ s32 state;
    /* 0x24 */ s32 cursor_record_36;
    /* 0x28 */ s32 field_28;
    /* 0x2c */ s32 field_2c;
    /* 0x30 */ s32 field_30;
    /* 0x34 */ s32 exiting;
    /* 0x38 */ u8 padding_38[0x64 - 0x38];
} open_title_menu_state_t;

/* Title-menu view of g_open_controller_stream_start; indexing it as an array of
 * this type (rather than casting the controller-record array) is required for
 * the target's constant-folded field addresses. */
extern open_title_menu_state_t g_open_title_menu_states[];

/* Build the title menu (logo, four items and cursor) and push its controller.
 *
 * The target builds the records base and the x/y view in v0 and copies them
 * into their saved registers. That needs the two short-lived temporaries
 * entry and field: each is referenced again later in the function, so CSE
 * keeps them as the canonical copies, and local allocation then rewrites
 * their remaining setup uses to records and positions. */
void open_title_push_menu_controller(s32 argument) {
    RECT rect;
    s32 unused[4];
    s32 i;
    s32 record;
    s32 y;
    s32 controller;
    open_render_record_36_position_t* positions;
    open_render_record_36_position_t* position;
    open_render_record_36_t* records;
    open_render_record_36_t* entry;
    s32* field;

    g_open_system_runtime_flags = (g_open_system_runtime_flags & ~0x2000) | 0x1000;
    open_gfx_init_screen_environments(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xf0;
    if (g_active_graphics_buffer_index != 0) {
        rect.y = 0xf0;
    }
    LoadImage(&rect, g_open_file_destination);
    DrawSync(0);

    record = open_gfx_append_render_record_36(
        (open_render_record_36_t**)g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    entry = g_open_gfx_render_records_36;
    records = entry;
    field = &records->x;
    positions = (open_render_record_36_position_t*)field;
    g_open_title_menu_states[g_open_current_controller_index].records_36[0] = record;
    g_open_gfx_render_records_36[record].anim_id = 0x11;
    g_open_gfx_render_records_36[record].ot_layer = 4;
    g_open_gfx_render_records_36[record].frame_timer = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    positions[record].x = 0;
    positions[record].y = 0x64;

    for (i = 0, y = 0x1e; i < 4; i++) {
        record = open_gfx_append_render_record_36(
            (open_render_record_36_t**)g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
        g_open_title_menu_states[g_open_current_controller_index].records_36[i + 1] = record;
        g_open_gfx_render_records_36[record].anim_id = i + 3;
        g_open_gfx_render_records_36[record].ot_layer = 4;
        records[record].frame_timer = 0;
        g_open_gfx_render_records_36[record].frame_index = 0;
        positions[record].y = y;
        y += 0xc;
        positions[record].x = -0x18;
    }

    record = open_gfx_append_render_record_36(
        (open_render_record_36_t**)g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_title_menu_states[g_open_current_controller_index].cursor_record_36 = record;
    entry = &g_open_gfx_render_records_36[record];
    entry->anim_id = 0x13;
    g_open_gfx_render_records_36[record].ot_layer = 4;
    g_open_gfx_render_records_36[record].frame_timer = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    g_open_gfx_render_records_36[record].flags |= OPEN_RENDER_RECORD_VISIBLE;
    position = (open_render_record_36_position_t*)&g_open_gfx_render_records_36[record].x;
    position->x = 0x60;
    position->y = 0x58;

    if (g_open_system_runtime_flags & 0x40) {
        open_gfx_start_overlay_fade_out(0x20);
    }

    controller = g_open_current_controller_index;
    g_open_title_menu_states[controller].selected_item = 0;
    g_open_title_menu_states[controller].idle_timer = 0;
    g_open_title_menu_states[controller].cd_end_position = argument;
    g_open_title_menu_states[controller].state = 0;
    g_open_title_menu_states[controller].exiting = 0;
    record = g_open_title_menu_states[controller].records_36[g_open_title_menu_states[controller].selected_item + 1];
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[record].palette = 9;
    field = &g_open_controller_handler_indices[controller];
    *field = 2;
}
