#include "fft/event_equip.h"
#include "psx/pad.h"

/*
 * Update the clear-all-equipment prompt and empty every occupied slot when accepted.
 *
 * Returns 1 while waiting for input and 0 after the prompt is accepted or dismissed.
 */
s32 equip_menu_update_clear_all_equipment_prompt(void) {
    struct {
        u8 _unused_00[0x40];
        s16 val;
    } scratch;
    s32 index;
    s32 value;
    s16* draw_value;
    /* Pinned: the target sets $a1 = &scratch.val at the loop head, before the
     * store; unpinned GCC forms it at the call. */
    register s32 call_argument_1 asm("$5");
    s16* draw_params;
    s32 flags;

    g_equip_text_help_message_id = -1;
    equip_gfx_set_otag_index(0x28);
    g_equip_menu_clear_all_prompt_frame = g_equip_menu_clear_all_prompt_frame + 1;
    if (g_equip_menu_clear_all_prompt_frame & 1) {
        index = 0;
        draw_value = &g_equip_menu_clear_all_prompt_draw_params[1];
        /* Keeps the draw_value address setup ahead of `li s1,0x90`. */
        __asm__ volatile("" : : "r"(draw_value));
        value = 0x90;
        draw_params = draw_value - 1;
        do {
            call_argument_1 = (s32)&scratch.val;
            /* Keeps the $a1 setup at the loop head, ahead of the store. */
            __asm__ volatile("" : : "r"(call_argument_1));
            *draw_value = value;
            value += 0x10;
            index++;
            scratch.val = 0;
            equip_menu_update_and_draw_animated_marker((RECT*)draw_params, (u16*)call_argument_1, g_event_mode);
            draw_params = draw_value - 1;
        } while (index < 5);
    }
    flags = g_equip_input_primary_repeat;
    if (flags & PSX_PAD_RIGHT) {
        g_equip_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (flags & PSX_PAD_CIRCLE) {
        value = 0;
        for (index = 0; index < 5; index++) {
            if (g_equip_unit_data[g_equip_unit_selected_index]->equipment[index] != 0) {
                value = 1;
                call_argument_1 = (s16)index;
                equip_unit_set_slot_item(g_equip_unit_selected_index, call_argument_1, 0);
            }
        }
        equip_unit_load_selected_data();
        if (value == 0) {
            g_equip_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_equip_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}
