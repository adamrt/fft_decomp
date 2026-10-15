#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

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
        if (!(g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE))) {
            world_shop_run_obtain_gil_menu();
            world_shop_run_soldier_office_fee_menu_script();
            world_menu_run_script_with_callback_suppressed();
            return;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_rename_error_shown = 0;
        g_world_shop_rename_prompt_initialized = 1;
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_background_visible = 0;
    }
    if (g_world_formation_unit_banner_enabled != 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_world_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_menu_help_message_text_id = text_id;
    } else {
        g_menu_help_message_text_id = 0;
    }
    world_formation_update_and_draw_unit_grid(
        1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode, world_shop_rename_unit_get_unit_marker, 0);
    if (g_world_shop_rename_error_shown != 0) {
        g_menu_help_message_text_id = -1;
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_rename_error_shown = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0 && world_formation_is_unit_list_scroll_needed() == 0) {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CIRCLE) {
            if (g_world_unit_pointers[g_unit_selected_index]->sprite_set == CHARACTER_IDENTITY_REIS_DRAGON) {
                if (g_world_formation_unit_banner_at_bottom != 0) {
                    mode = 1;
                } else {
                    mode = 2;
                }
                thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, mode | 0x18, 0xF848, 0);
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
                g_world_shop_rename_error_shown = 1;
            } else {
                g_world_shop_menu_step = WORLD_SHOP_STEP_RENAME_NAME_ENTRY;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                g_world_formation_unit_banner_enabled = 0;
                g_world_shop_rename_prompt_initialized = 0;
            }
        } else if (input & PSX_PAD_CROSS) {
            g_world_shop_background_visible = 1;
            g_world_shop_rename_prompt_initialized = 0;
            g_world_formation_unit_banner_enabled = 0;
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    }
}
