#include "fft/wldcore.h"
#include "psx/gs.h"

void world_gs_gettiminfo(u32* tim, GsIMAGE* image);

/* World-script opcode: loads picture operand_0 through 0x80068ab4, places
 * its render record at (operand_2 - 0x80, operand_1 - 0x78), uploads the
 * TIM's pixels and CLUT, and binds the render record to the VRAM image. */
void wldcore_opcode_load_picture(void) {
    GsIMAGE image;
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t clut_position;
    wldcore_point32_t unused_slot; /* reserves the target frame's 0x50 slot */
    u32 instruction;
    s32 picture;
    s32 x;
    s32 y;

    instruction = g_wldcore_opcode_state.instruction.word;
    x = instruction >> 24;
    y = (instruction >> 16) & 0xFF;
    picture = (instruction & 0xFF00) >> 8;
    g_wldcore_opcode_state.picture = picture;
    g_wldcore_opcode_state.x = x;
    g_wldcore_opcode_state.y = y;
    wldcore_proposition_load_picture_data_set(picture);
    wldcore_wait_for_file_load();
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].base_x = x - 0x80;
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].base_y = y - 0x78;
    world_gs_gettiminfo(g_wldcore_picture_buffer + 1, &image);
    bounds.position.x = image.px;
    bounds.position.y = image.py;
    bounds.dimensions.x = image.pw;
    bounds.dimensions.y = image.ph;
    clut_position.x = image.cx;
    clut_position.y = image.cy;
    LoadTPage(image.pixel, 0, 0, (s16)bounds.position.x, (s16)bounds.position.y, (s16)bounds.dimensions.x * 4,
        (s16)bounds.dimensions.y);
    LoadClut(image.clut, clut_position.x, clut_position.y);
    wldcore_window_init_vram_render_record(
        g_wldcore_opcode_state.render_index, bounds.position, bounds.dimensions, clut_position, 4);
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].anim_counter = 0;
    g_wldcore_opcode_state.state_flags |= 4;
}
