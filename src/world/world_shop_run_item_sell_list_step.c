#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef s32 (*world_menu_column_callback_t)(void);

/* Item-list shop menu step: on first entry prompts 0xF80B (0xF80C when the
 * player already holds items), rebuilds the item list and installs the seven
 * column callbacks, then handles cancel (0x40) and confirm (0x20).
 *
 * As in its structural twin world_shop_run_item_sell_confirm_step, the
 * constant is pinned into $v0 (free on an immediate assignment) so the arm's
 * result inherits that register, and the *addend* rather than the arm's
 * result is laundered so the increment must be recomputed.
 */
void world_shop_run_item_sell_list_step(void) {
    s32 category;
    s32 next_category;
    s32 message;

    if (g_world_shop_sell_list_initialized == 0) {
        /* The constant wins $v0 and dies at the beq; the arm's result then takes
         * that register by ordinary allocation, as the target has it. Only the
         * immediate assignment is pinned, so the pin is free. */
        register s32 four __asm__("$2");

        world_thread_set_parameters(2, 0x19, world_menu_has_items(0) != 0 ? 0xF80C : 0xF80B, 0);
        g_world_shop_item_category = world_menu_set_cursor_position_2(0, 0);
        /* The target compares against a register (li v0,4 / beq v1,v0) and
         * materialises it ahead of both loads. */
        four = 4;
        category = g_world_shop_item_category;
        /* The target reloads the category for the == 4 test (lbu a0 then
         * lbu v1); cse otherwise shares the first read. */
        __asm__ volatile("" : : : "memory");
        /* Computed before the test so it lands in the beq delay slot, as the
         * target has it. */
        next_category = category + 1;
        if (g_world_shop_item_category != four) {
            /* The target recomputes category + 1 here rather than reusing the
             * value above, into the register the constant just freed. Laundering
             * the ADDEND (not the result) is what forces the recompute: a self-tie
             * on the result lets cse substitute the dominator's equal value into
             * the definition itself, leaving only a copy. */
            s32 shifted;
            __asm__("" : "=r"(category) : "0"(category));
            shifted = category + 1;
            message = (s8)shifted;
        } else {
            message = 7;
        }
        world_item_build_category_list(-1, next_category, message, (world_item_list_entry_t*)g_world_menu_entry_ids, 0);
        world_menu_init_scrollable_list_core(
            g_world_menu_entry_ids, g_world_menu_cursor_position, g_world_text_item_names);
        g_world_menu_script_callbacks[0] = world_menu_get_entry_item_count_2;
        g_world_menu_script_callbacks[1] = world_menu_get_entry_item_count;
        g_world_menu_aux_callback = (world_menu_column_callback_t)world_item_build_type_icon_source;
        g_world_menu_script_callbacks[3] = (s32 (*)(s32))world_menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4] = (s32 (*)(s32))world_item_get_selected_half_price;
        g_world_menu_script_callbacks[5] = world_menu_get_entry_item_count_2;
        g_world_menu_script_callbacks[6] = world_menu_get_entry_item_count;
        g_world_shop_sell_list_initialized = 1;
    }
    world_script_set_vsync_mode_and_event_speed(2);
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_shop_sell_list_initialized = 0;
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        world_thread_set_parameters(2, 0x19, 0xF802, 0);
        g_world_shop_menu_step = 0;
    } else if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_world_menu_option_count == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
        } else {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            g_world_shop_quantity_menu_data.selection = 0;
            g_world_shop_menu_step = 5;
        }
    }
    world_menu_switch_item_category(g_world_input_secondary_repeat);
    world_menu_run_script_with_palette_mode(
        g_world_shop_sell_list_menu_script, g_world_input_primary_repeat, g_world_thread_task_active);
    world_shop_run_obtain_gil_menu();
}
