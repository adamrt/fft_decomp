#include "fft/event_option.h"
#include "psx/types.h"

/* g_menu_text_state is used as one aggregate (menu_text_state_t): that is
 * what reproduces the target's single s5 base register and the
 * MEM_IN_STRUCT_P reordering of the g_menu_text_palette_offset load across the color
 * store. */
/* option_menu_text_layout_t and option_menu_entry_t live in fft/event_option.h. */

void option_menu_render_entries(option_menu_entry_t* entry, s32* row_offset, void* buffer) {
    option_menu_text_layout_t* layout = entry->text_binding;
    s32 row;
    s32 column;
    s32 index;

    for (row = 0; row < layout->row_count; row++) {
        g_menu_text_state.stride = entry->inner_width;
        for (column = 0; column < 3; column++) {
            index = row + *row_offset;
            if (layout->mode[column] == 2) {
                continue;
            }
            if ((layout->text_ids[column][index] & 0x7FF) == 0x7FF) {
                continue;
            }
            /* The target passes both coordinates without the s16 sign extension. */
            ((void (*)(s32, s32))battle_menu_set_text_origin)(layout->x[column], row * 0x10);
            if (layout->mode[column] != 2) {
                g_menu_text_palette_offset = layout->text_colors[column][index];
            }
            g_menu_text_state.color = 0;
            if (g_menu_text_palette_offset == 4) {
                g_menu_text_state.color = 0x44444444;
            }
            if (layout->mode[column] == 0) {
                battle_menu_display_text_entry(layout->text_ids[column][index], buffer, &g_menu_text_state.origin_x);
            } else if (layout->mode[column] == 1) {
                battle_display_menu_number_entry(
                    layout->text_ids[column][index], 2, (s32)buffer, (u16*)&g_menu_text_state.origin_x);
            } else if (layout->mode[column] == 3) {
                battle_draw_menu_number_glyphs(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.origin_x);
            }
            g_menu_text_palette_offset = 0;
            g_menu_text_state.color = 0;
        }
    }
}
