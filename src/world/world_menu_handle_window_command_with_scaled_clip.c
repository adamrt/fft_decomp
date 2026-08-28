#include "fft/world.h"

/*
 * Apply the centered opening-animation clip before processing a window command.
 *
 * Steps 0-3 scale width and height by the signed percentage table. Later
 * steps disable the temporary clip; the downstream command still runs.
 */
void world_menu_handle_window_command_with_scaled_clip(world_menu_window_command_t* command) {
    RECT clip;
    s32 scale;

    if (g_world_menu_window_scale_step < 4) {
        if (g_world_menu_window_scale_step < 3) {
            g_world_menu_display_script_input = 0;
        }
        scale = g_world_menu_window_scale_table[g_world_menu_window_scale_step];
        clip.w = command->width * scale / 100;
        clip.h = command->height * scale / 100;
        clip.x = command->x + (command->width >> 1) - (clip.w >> 1);
        clip.y = command->y + (command->height >> 1) - (clip.h >> 1);
        world_gfx_add_draw_area_primitive(&clip, g_world_menu_draw_priority - 1);
        g_world_menu_temporary_draw_area_active = 1;
    } else {
        g_world_menu_temporary_draw_area_active = 0;
    }
    world_menu_parse_draw_window_frame(command);
}
