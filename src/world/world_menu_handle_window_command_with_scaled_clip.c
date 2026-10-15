#include "fft/world.h"

/*
 * Apply the centered opening-animation clip before processing a window command.
 *
 * Steps 0-3 scale width and height by the signed percentage table. Later
 * steps disable the temporary clip; the downstream command still runs.
 */
menu_instruction_t* world_menu_handle_window_command_with_scaled_clip(menu_instruction_t* instruction) {
    RECT clip;
    s32 scale;

    if (g_gfx_transition_frame < 4) {
        if (g_gfx_transition_frame < 3) {
            g_menu_cmd_stream_input = 0;
        }
        scale = g_world_menu_window_scale_table[g_gfx_transition_frame];
        clip.w = instruction->window.width * scale / 100;
        clip.h = instruction->window.height * scale / 100;
        clip.x = instruction->window.x + (instruction->window.width >> 1) - (clip.w >> 1);
        clip.y = instruction->window.y + (instruction->window.height >> 1) - (clip.h >> 1);
        world_gfx_add_draw_area_primitive(&clip, g_menu_gfx_otag_index - 1);
        g_gfx_otag_index_locked = 1;
    } else {
        g_gfx_otag_index_locked = 0;
    }
    return world_menu_parse_draw_window_frame(instruction);
}
