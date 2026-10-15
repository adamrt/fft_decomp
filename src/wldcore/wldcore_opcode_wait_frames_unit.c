#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Sound-novel opcode: wait operand 0 frames. The first pass (state bit
 * 0x10) latches the frame count and clears the counter; each later pass
 * yields until the counter has passed the count, then asks for the next
 * instruction. */
void wldcore_opcode_wait_frames(void) {
    u16 flags;
    u16 updated;
    s32 counter;
    s32 operand;

    flags = g_wldcore_active_saved_record.state_flags;
    updated = flags | WLDCORE_OPCODE_STATE_YIELD;
    g_wldcore_active_saved_record.state_flags = updated;
    if ((updated & WLDCORE_OPCODE_STATE_FETCHED) != 0) {
        g_wldcore_sound_novel_wait_counter = 0;
        g_wldcore_sound_novel_wait_frames = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    }
    counter = g_wldcore_sound_novel_wait_counter;
    operand = g_wldcore_sound_novel_wait_frames;
    if (operand < counter) {
        g_wldcore_active_saved_record.state_flags = flags | (WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_YIELD);
    } else {
        g_wldcore_sound_novel_wait_counter = counter + 1;
    }
}

/* World-script opcode: loads picture operand_0 through 0x80068ab4, places
 * its render record at (operand_2 - 0x80, operand_1 - 0x78), uploads the
 * TIM's pixels and CLUT, and binds the render record to the VRAM image. */
void wldcore_opcode_load_picture(void) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t unused_slot; /* reserves the target frame's 0x50 slot */
    u32 instruction;
    s32 picture;
    s32 x;
    s32 y;

    instruction = g_wldcore_opcode_state.instruction;
    x = instruction >> 24;
    y = (instruction >> 16) & 0xFF;
    picture = (instruction & 0xFF00) >> 8;
    g_wldcore_opcode_state.picture = picture;
    g_wldcore_opcode_state.x = x;
    g_wldcore_opcode_state.y = y;
    wldcore_proposition_load_picture_data_set(picture);
    file_wait_for_pending_variant();
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].base.x = x - 0x80;
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].base.y = y - 0x78;
    GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
    bounds.x = image.px;
    bounds.y = image.py;
    bounds.w = image.pw;
    bounds.h = image.ph;
    clut_position.x = image.cx;
    clut_position.y = image.cy;
    LoadTPage(image.pixel, 0, 0, bounds.x, bounds.y, bounds.w * 4, bounds.h);
    LoadClut(image.clut, clut_position.x, clut_position.y);
    wldcore_window_init_vram_render_record(
        g_wldcore_opcode_state.render_index, bounds, clut_position, WORLD_RENDER_RECORD_8BIT_CLUT);
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].anim_counter = 0;
    g_wldcore_opcode_state.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Hides the sound-novel picture's render record and forgets the picture. */
void wldcore_opcode_hide_picture(void) {
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_active_saved_record.picture_id = -1;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_YIELD;
}

/* Script opcode: operand byte 0 sets (non-zero) or clears state flag 0x40
 * and shows or hides both windows of the parent menu level (window flag
 * 0x10), then sets state flag 0x04. */
void wldcore_opcode_set_parent_windows_visible(void) {
    if ((u8)(g_wldcore_opcode_state.instruction >> 8) != 0) {
        g_wldcore_opcode_state.state_flags |= WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.first_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.second_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_opcode_state.state_flags &= ~WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.first_window]
            .flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.second_window]
            .flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
    g_wldcore_opcode_state.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}
