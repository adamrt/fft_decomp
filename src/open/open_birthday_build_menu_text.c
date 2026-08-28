#include "fft/open.h"
#include "fft/world.h"
#include "psx/types.h"

/* Rebuild the selected birthday's month and day in the lower menu window.
 *
 * The month label is centred from its measured width; both entries are drawn
 * into one scratch image before it is uploaded to VRAM. */
void open_birthday_build_menu_text(open_birthday_date_state_t* menu) {
    RECT quad;
    open_point32_t position;
    u16 width;
    u16 height;
    s32 index;
    s32 text_id;
    open_render_record_56_position_t* translation;
    s32* substitution;
    u16 window_width;

    index = open_gfx_append_render_record_56(
        (open_render_record_56_t**)g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    menu->window_record_56 = index;
    quad.x = 0;
    quad.y = 0x40;
    quad.w = 0x50;
    quad.h = 0x20;
    open_gfx_init_window_frame_record(index, quad, 0, (u16*)g_open_birthday_window_image);

    text_id = menu->month + 0xB802;
    world_text_measure_pixels(&width, &height, world_text_find_entry(text_id));

    position.x = 0x1C - (s16)width / 2;
    position.y = 8;
    substitution = g_world_text_substitution_values;
    *substitution = text_id;
    open_birthday_draw_menu_text_entry(index, 0xB801, position, g_open_birthday_window_image);

    position.x = 0x30;
    position.y = 8;
    *substitution = menu->day;
    open_birthday_draw_menu_text_entry(index, 0xB802, position, g_open_birthday_window_image);

    open_birthday_load_window_image(index, (u32*)g_open_birthday_window_image);

    translation = g_open_gfx_record_translation;
    translation += index;
    window_width = quad.w;
    translation->y = -0x10;
    translation->x = -((s16)window_width / 2);
    g_open_gfx_render_records_56[index].ot_layer = 4;
    g_open_gfx_render_records_56[index].flags &= ~0x100;
}
