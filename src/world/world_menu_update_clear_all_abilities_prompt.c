#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* volatile: the target re-reads the counter right after incrementing it */

s32 world_menu_update_clear_all_abilities_prompt(void) {
    world_formation_unit_t* unit;
    u16 value;
    s32 unused[16];
    s32 i;
    s32 changed;

    g_world_menu_description_text_id = -1;
    world_menu_set_draw_priority(0x28);
    g_world_clear_abilities_blink_counter = g_world_clear_abilities_blink_counter + 1;
    if (g_world_clear_abilities_blink_counter & 1) {
        for (i = 1; i < 5; i++) {
            g_world_clear_abilities_cursor_point.y = i * 0x10 + 0x90;
            value = 0;
            world_menu_draw_animated_cursor(&g_world_clear_abilities_cursor_point, &value, g_world_thread_task_active);
        }
    }
    if (g_world_input_primary_repeat & PSX_PAD_RIGHT) {
        g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        changed = 0;
        for (i = 1; i < 5; i++) {
            unit = g_world_formation_unit_pointers[g_world_formation_selected_unit_index];
            if (((world_formation_unit_ability_slots_t*)unit)->ability_slots[i] != 0) {
                changed = 1;
                world_formation_set_unit_ability_slot(g_world_formation_selected_unit_index, i, 0,
                    world_ability_is_equip_support_change(g_world_formation_selected_unit_index,
                        (s16)((world_formation_unit_ability_slots_t*)unit)->ability_slots[i]));
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
