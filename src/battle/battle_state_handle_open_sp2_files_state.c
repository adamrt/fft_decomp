#include "fft/battle.h"
#include "psx/types.h"

/* OpenSp2Files game-state handler: load the SP2 sheet holding the casting unit's
 * attack animation, then start the strike
 * (action_set_damage_display_type_based_on_ability).
 *
 * With no buffer yet, a 0x8000-byte buffer is allocated and the read requested. A
 * unit that needs no SP2 file frees the buffer and starts the strike at once.
 * Retail also frees the buffer when the request was accepted (0) and keeps it
 * when the loader was busy (1): the next frame allocates again, that request is
 * refused, and the file arrives in the kept block only because the heap hands the
 * freed pages back. Once the file has loaded, the first free of the two VRAM
 * slots (owner 0xff) takes the unit's id | 0x40 as owner, the 64x256 image is
 * uploaded to the slot's rectangle and the strike starts. While neither slot is
 * free the state waits. */
void battle_state_handle_open_sp2_files_state(void) {
    RECT image_rect;
    battle_unit_t* unit;
    s32 open_result;
    s32 counter;
    s32 empty_slot;
    s32 offset;
    RECT* image_rect_address;
    u32* sp2_data;

    unit = battle_unit_get_casting();
    if (g_battle_gfx_sp2_data == 0) {
        g_battle_gfx_sp2_data = main_heap_alloc(0x8000);
        open_result = battle_gfx_open_sp2_file(unit, g_battle_gfx_sp2_data);
        if (open_result == -1) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
            action_set_damage_display_type_based_on_ability();
        } else if (open_result == 0) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
        }
    } else if (main_file_is_still_loading() == 0) {
        counter = 0;
        empty_slot = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        /* Keep the target's byte-offset induction; advancing a slot pointer changes the exact code. */
        offset = 0;
        do {
            if (((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->owner == empty_slot) {
                image_rect_address = &image_rect;
                sp2_data = g_battle_gfx_sp2_data;
                ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->owner
                    = unit->runtime_id | BATTLE_GFX_VRAM_SLOT_OWNER_SP2;
                image_rect_address->x
                    = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->image_rect.x;
                image_rect_address->y
                    = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->image_rect.y;
                image_rect_address->w = 0x40;
                image_rect_address->h = 0x100;
                LoadImage(image_rect_address, sp2_data);
                action_set_damage_display_type_based_on_ability();
                break;
            }
            counter++;
            offset += 0x7564;
        } while (counter < 2);
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}
