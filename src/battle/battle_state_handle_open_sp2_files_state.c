#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"

void battle_state_handle_open_sp2_files_state(void) {
    RECT image_rect;
    battle_unit_misc_data_t* unit;
    s32 open_result;
    s32 counter;
    s32 empty_slot;
    s32 offset;
    RECT* image_rect_address;
    void* sp2_data;

    unit = battle_unit_get_casting_misc_data();
    if (g_battle_gfx_sp2_data == 0) {
        g_battle_gfx_sp2_data = game_malloc(0x8000);
        open_result = battle_gfx_open_sp2_file(unit, g_battle_gfx_sp2_data);
        if (open_result == -1) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
            battle_action_set_damage_display_type_based_on_ability();
        } else if (open_result == 0) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
        }
    } else if (main_file_is_still_loading() == 0) {
        counter = 0;
        empty_slot = 0xff;
        /* Keep the target's byte-offset induction; advancing a slot pointer changes the exact code. */
        offset = 0;
        do {
            if (((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->owner == empty_slot) {
                image_rect_address = &image_rect;
                sp2_data = g_battle_gfx_sp2_data;
                ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->owner = unit->unit_id | 0x40;
                image_rect_address->x
                    = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->image_rect.x;
                image_rect_address->y
                    = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->image_rect.y;
                image_rect_address->w = 0x40;
                image_rect_address->h = 0x100;
                LoadImage(image_rect_address, sp2_data);
                battle_action_set_damage_display_type_based_on_ability();
                break;
            }
            counter++;
            offset += 0x7564;
        } while (counter < 2);
    }
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
}
