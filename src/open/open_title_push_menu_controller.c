#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Title-menu view of g_open_controller_records_next; indexing it as an array of
 * this type (rather than casting the controller-record array) is required for
 * the target's constant-folded field addresses. */
extern open_title_controller_t g_open_title_menu_states[];

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
    wldcore_window_xy_t* positions;
    point32_t* position;
    world_animated_render_record_t* records;
    world_animated_render_record_t* entry;
    s32* field;

    g_open_system_runtime_flags
        = (g_open_system_runtime_flags & ~OPEN_SYSTEM_RUNTIME_FLAG_RGB24) | OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
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

    record = window_append_animated_record_variant(
        g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    entry = g_open_gfx_render_records_36;
    records = entry;
    field = &records->position.x;
    positions = (wldcore_window_xy_t*)field;
    g_open_title_menu_states[g_open_current_controller_index].records_36[0] = record;
    g_open_gfx_render_records_36[record].sequence = 0x11;
    g_open_gfx_render_records_36[record].priority = 4;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    positions[record].x = 0;
    positions[record].y = 0x64;

    for (i = 0, y = 0x1e; i < 4; i++) {
        record = window_append_animated_record_variant(
            g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
        g_open_title_menu_states[g_open_current_controller_index].records_36[i + 1] = record;
        g_open_gfx_render_records_36[record].sequence = i + 3;
        g_open_gfx_render_records_36[record].priority = 4;
        records[record].anim_counter = 0;
        g_open_gfx_render_records_36[record].frame_index = 0;
        positions[record].y = y;
        y += 0xc;
        positions[record].x = -0x18;
    }

    record = window_append_animated_record_variant(
        g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_title_menu_states[g_open_current_controller_index].cursor_record_36 = record;
    entry = &g_open_gfx_render_records_36[record];
    entry->sequence = 0x13;
    g_open_gfx_render_records_36[record].priority = 4;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    g_open_gfx_render_records_36[record].flags |= WORLD_RENDER_RECORD_HIDDEN;
    position = &g_open_gfx_render_records_36[record].position;
    position->x = 0x60;
    position->y = 0x58;

    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED) {
        open_gfx_start_overlay_fade_out(0x20);
    }

    controller = g_open_current_controller_index;
    g_open_title_menu_states[controller].cursor = OPEN_TITLE_MENU_NEW_GAME;
    g_open_title_menu_states[controller].idle_timer = 0;
    g_open_title_menu_states[controller].cd_end_position = argument;
    g_open_title_menu_states[controller].state = OPEN_TITLE_STATE_MENU;
    g_open_title_menu_states[controller].exiting = 0;
    record = g_open_title_menu_states[controller].records_36[g_open_title_menu_states[controller].cursor + 1];
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[record].palette = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
    field = &g_open_controller_handler_indices[controller];
    *field = OPEN_CONTROLLER_TITLE_MENU;
}
