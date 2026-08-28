#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

extern void world_formation_update_and_draw_unit_grid(s32, s32, s32, s32, s32, s32 (*)(void), s32);

/*
 * Menu step for the rename-unit prompt: on the first frame either leaves
 * the screen when no face button is held, or arms the prompt. Confirm
 * opens the name entry (mode 1 when a unit is already renamed) unless the
 * unit has Reis's dragon identity, which refuses with message 0x1B; cancel backs
 * out with message 0x19.
 */
void world_shop_run_rename_unit_prompt_step(void) {
    s32 mode;
    s32 input;
    s32 text_id;

    if (g_world_shop_rename_prompt_initialized == 0) {
        if (!(g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE))) {
            world_shop_run_obtain_gil_menu();
            world_shop_run_soldier_office_fee_menu_script();
            world_menu_run_script_with_callback_suppressed();
            return;
        }
        world_thread_set_parameters(2, 0x19, -1, 0);
        g_world_shop_rename_error_shown = 0;
        g_world_shop_rename_prompt_initialized = 1;
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_background_visible = 0;
    }
    if (g_world_formation_unit_banner_enabled != 0) {
        text_id = 0x20000;
        if (g_world_formation_scroll_enabled != 0) {
            text_id = 0x20001;
        }
        g_world_menu_description_text_id = text_id;
    } else {
        g_world_menu_description_text_id = 0;
    }
    world_formation_update_and_draw_unit_grid(1, 1, 0, (s16)g_world_input_secondary_repeat,
        *(s16*)&g_world_thread_task_active, world_shop_rename_unit_get_unit_marker, 0);
    if (g_world_shop_rename_error_shown != 0) {
        g_world_menu_description_text_id = -1;
        if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_rename_error_shown = 0;
            world_thread_set_parameters(2, 0x19, -1, 0);
        }
    }
    if (world_thread_is_running(2) == 0 && world_formation_can_scroll_slots_back() == 0) {
        input = g_world_input_primary_repeat;
        if (input & PSX_PAD_CIRCLE) {
            if (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->sprite_set
                == CHARACTER_IDENTITY_REIS_DRAGON) {
                if (g_world_formation_scroll_enabled != 0) {
                    mode = 1;
                } else {
                    mode = 2;
                }
                world_thread_start(2, world_text_message_box_thread);
                world_thread_set_parameters(2, mode | 0x18, 0xF848, 0);
                g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                g_world_shop_rename_error_shown = 1;
            } else {
                g_world_shop_menu_step = 0x1B;
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                g_world_formation_unit_banner_enabled = 0;
                g_world_shop_rename_prompt_initialized = 0;
            }
        } else if (input & PSX_PAD_CROSS) {
            g_world_shop_background_visible = 1;
            g_world_shop_rename_prompt_initialized = 0;
            g_world_formation_unit_banner_enabled = 0;
            world_thread_start(2, world_text_message_box_thread);
            world_thread_set_parameters(2, 0x19, 0xF821, 0);
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_shop_menu_step = 0x19;
        }
    }
}
