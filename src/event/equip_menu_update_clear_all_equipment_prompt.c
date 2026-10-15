#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/pad.h"

/*
 * Update the clear-all-equipment prompt and empty every occupied slot when accepted.
 *
 * Returns 1 while waiting for input and 0 after the prompt is accepted or dismissed.
 */
s32 equip_menu_update_clear_all_equipment_prompt(void) {
    u8 unused_buffer[0x40]; /* unreferenced; reserves the target's frame */
    u16 marker_state;
    s32 index;
    s32 unequipped;
    s32 flags;

    g_equip_text_help_message_id = -1;
    equip_gfx_set_otag_index(0x28);
    g_equip_menu_clear_all_prompt_frame = g_equip_menu_clear_all_prompt_frame + 1;
    if (g_equip_menu_clear_all_prompt_frame & 1) {
        for (index = 0; index < 5; index++) {
            g_equip_menu_clear_all_prompt_draw_params.y = 0x90 + index * 0x10;
            marker_state = 0;
            menu_update_and_draw_animated_cursor(
                &g_equip_menu_clear_all_prompt_draw_params, &marker_state, g_menu_input_lock_mode);
        }
    }
    flags = g_menu_input_primary_repeat;
    if (flags & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (flags & PSX_PAD_CIRCLE) {
        unequipped = 0;
        for (index = 0; index < 5; index++) {
            if (g_equip_unit_data[g_equip_unit_selected_index]->equipment[index] != 0) {
                unequipped = 1;
                equip_unit_set_slot_item(g_equip_unit_selected_index, index, 0);
            }
        }
        equip_unit_load_selected_data();
        if (unequipped == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}
