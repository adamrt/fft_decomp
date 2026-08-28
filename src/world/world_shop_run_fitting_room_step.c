#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* 0x801cd72a and 0x801cd72c are separately catalogued halfwords inside the
 * menu selection results. The target holds a base register for the first
 * (two `lh 0(s0)` reads) and addresses the second absolutely, which is the
 * pointer-local-plus-absolute split src/world/world_formation_item_menu.c
 * already uses on this same pair. */

/* Accepted spelling with a void callback
 * (src/world/world_shop_run_rename_unit_prompt_step.c); the sibling that
 * passes two s32 callbacks declares the other form. */
extern void world_formation_update_and_draw_unit_grid(s32, s32, s32, s32, s32, s32 (*)(void), s32);

/* Fitting-room menu step: on entry clear the trial equipment and its cost;
 * then either browse the formation (confirm opens the fitting menu, cancel
 * returns to shop step 0xC) or run the fitting menu thread and dispatch its
 * selection to the next shop step. */
void world_shop_run_fitting_room_step(void) {
    s32 busy;
    s32 chosen;
    /* Pin: holds the 4 in $v0 ahead of the flag stores; unpinned, GCC loads it after them. */
    register s32 exit_choice __asm__("$2");
    s16* selection;

    if (g_world_shop_fitting_room_initialized == 0) {
        if (g_world_input_newly_pressed == 0) {
            world_shop_run_obtain_gil_menu();
            return;
        }
        world_formation_init_cursor_and_scroll_state(0);
        memset(g_world_shop_fitting_room_items, 0, 0xC8);
        world_shop_add_fitting_room_cost(-world_shop_add_fitting_room_cost(0));
        g_world_shop_fitting_room_unit_chosen = 0;
        g_world_shop_cost_window_visible = 0;
        world_thread_set_parameters(2, 0x19, -1, 0);
        g_world_shop_fitting_room_initialized = 1;
        return;
    }
    if (g_world_shop_fitting_room_unit_chosen == 0) {
        g_world_shop_background_visible = 0;
        if (g_world_formation_unit_banner_enabled != 0) {
            g_world_menu_description_text_id = (g_world_formation_scroll_enabled != 0) ? 0x20001 : 0x20000;
        } else {
            g_world_menu_description_text_id = 0;
        }
        busy = world_input_get_lockout_timer();
        busy += world_formation_can_scroll_slots_back();
        if ((g_world_input_primary_repeat & PSX_PAD_CROSS) && busy == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = 0xC;
        } else if ((g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE)) && busy == 0) {
            g_world_formation_unit_cycle_mode = 2;
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            g_world_shop_fitting_room_unit_chosen = 1;
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_scroll_enabled = 0;
            g_world_shop_cost_window_visible = 1;
            g_world_shop_fitting_room_menu_thread_data[0].cursor = 0;
            g_world_shop_fitting_room_menu_thread_data[1].cursor = 0;
            world_gfx_clear_sprite_slots();
        } else {
            world_formation_update_and_draw_unit_grid(1, 1, 0, (s16)g_world_input_secondary_repeat,
                *(s16*)&g_world_thread_task_active, world_shop_fitting_room_get_unit_marker, 0);
        }
    } else {
        g_world_shop_background_visible = 1;
        g_world_menu_description_text_id = 0;
        if (world_menu_run_thread(6, g_world_shop_fitting_room_menu_thread_data) == 0) {
            selection = &g_world_menu_selection_results[1];
            if (*selection == 0) {
                g_world_shop_menu_step = 7;
            } else if (*selection == 1) {
                g_world_shop_menu_step = 8;
            } else if (*selection == 2) {
                g_world_shop_menu_step = 9;
            } else if (*selection == 3) {
                if (g_world_menu_selection_results[2] == 0) {
                    g_world_shop_menu_step = 0xA;
                } else {
                    g_world_shop_menu_step = 0xB;
                }
            } else {
                g_world_shop_fitting_room_unit_chosen = 0;
                world_formation_init_selection_cursor(g_world_formation_selected_unit_index);
                chosen = *selection;
                exit_choice = 4;
                g_world_formation_unit_cycle_mode = 0;
                g_world_formation_unit_browse_enabled = 0;
                g_world_shop_cost_window_visible = 0;
                if (chosen == exit_choice) {
                    g_world_formation_unit_banner_enabled = 0;
                    g_world_shop_menu_step = 0xC;
                }
            }
            world_menu_set_cursor_position(3, 0);
            world_menu_set_cursor_position(2, 0);
        }
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}
