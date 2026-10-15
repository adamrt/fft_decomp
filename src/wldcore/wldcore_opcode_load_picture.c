#include "fft/wldcore.h"
#include "psx/libgs.h"

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
