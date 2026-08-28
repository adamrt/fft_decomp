#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Handle the remove-all-equipment prompt for the selected formation unit.
 *
 * Redraw the five slot markers on alternate updates. Confirm removes occupied
 * slots; Right exits the prompt. Both paths request the corresponding sound.
 * Return 1 while awaiting input and 0 when the prompt has finished.
 */
s32 world_menu_handle_remove_all_equipment(void) {
    u16 value;
    /* The target reserves 64 additional local bytes; their purpose is unknown. */
    s32 unused[16];
    s32 i;
    s32 changed;

    g_world_menu_description_text_id = -1;
    world_menu_set_draw_priority(0x28);
    g_world_remove_all_equipment_blink_counter = g_world_remove_all_equipment_blink_counter + 1;
    if (g_world_remove_all_equipment_blink_counter & 1) {
        for (i = 0; i < 5; i++) {
            g_world_remove_all_equipment_cursor_point.y = i * 0x10 + 0x90;
            value = 0;
            world_menu_draw_animated_cursor(
                &g_world_remove_all_equipment_cursor_point, &value, g_world_thread_task_active);
        }
    }
    if (g_world_input_primary_repeat & PSX_PAD_RIGHT) {
        g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        changed = 0;
        for (i = 0; i < 5; i++) {
            if (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[i]
                != ITEM_ID_NOTHING) {
                changed = 1;
                world_formation_equip_item_to_unit_slot(g_world_formation_selected_unit_index, i, ITEM_ID_NOTHING);
            }
        }
        world_formation_stage_selected_unit();
        if (changed == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
        } else {
            g_world_menu_sound_effect_id = MAIN_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}
