/*
 * Extended BATTLE counterpart of
 * world_draw_menu_text_columns_narrow (WORLD 0x800ed5f4).
 */
#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_text.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

struct battle_menu_text_position;
extern void battle_menu_draw_stacked_glyph_pair(void* buffer, struct battle_menu_text_position* origin);

/* Draw the visible rows of a three-column menu text layout (text 8 pixels
 * narrower than the entry). Text columns append text 0x5088 for rows whose
 * skill flag bit 0 is set while the current thread's fourth word is non-zero;
 * alternate-number columns draw a special glyph for values 0/1 without a
 * target, and text 0x509d for values of 100 and above. */
void battle_menu_draw_text_columns_narrow(world_menu_entry_t* entry, s32* row_offset, void* buffer) {
    world_menu_text_layout_t* layout = (world_menu_text_layout_t*)entry->text_binding;
    s32 row;
    s32 column;
    s32 index;
    s32 value;

    for (row = 0; row < layout->row_count; row++) {
        g_menu_text_state.stride = entry->inner_width - 8;
        for (column = 0; column < 3; column++) {
            index = row + *row_offset;
            if (layout->mode[column] == 2) {
                continue;
            }
            /* The target passes x and y as full words; the s16 prototype narrows them. */
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
                if (g_battle_threads[g_battle_current_thread_id].function_parameter_4 != 0
                    && (((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->flags[index] & 1)) {
                    g_menu_text_palette_offset = layout->text_colors[column][index];
                    /* Full-word x and y, as above. */
                    ((void (*)(s32, s32))battle_menu_set_text_origin)(layout->x[column] + 0x50, row * 0x10);
                    battle_menu_display_text_entry(0x5088, buffer, &g_menu_text_state.origin_x);
                }
            } else if (layout->mode[column] == 1) {
                battle_display_menu_number_entry(
                    layout->text_ids[column][index], 2, (s32)buffer, (u16*)&g_menu_text_state.origin_x);
            } else if (layout->mode[column] == 3) {
                if ((column == 2 || (column == 1 && layout->mode[2] == 2)) && (u16)layout->text_ids[column][index] < 2
                    && (((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->target[index] == 0xFF
                        || ((world_ability_skill_use_tables_t*)g_battle_ai_workspace_ptr)->target[index] == 0)) {
                    g_menu_text_state.origin_y += 2;
                    battle_menu_draw_stacked_glyph_pair(
                        buffer, (struct battle_menu_text_position*)&g_menu_text_state.origin_x);
                } else {
                    value = (u16)layout->text_ids[column][index];
                    if (value & 0x7FF) {
                        if ((value & 0x7FF) >= 100) {
                            g_menu_text_palette_offset = 0;
                            g_menu_text_state.origin_x += 3;
                            battle_menu_display_text_entry(0x509D, buffer, &g_menu_text_state.origin_x);
                        } else {
                            battle_draw_menu_number_glyphs(
                                (s16)value, 2, buffer, (battle_rect_t*)&g_menu_text_state.origin_x);
                        }
                    }
                }
            }
            g_menu_text_palette_offset = 0;
            g_menu_text_state.color = 0;
        }
    }
}
