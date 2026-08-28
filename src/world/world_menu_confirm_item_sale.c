#include "fft/world.h"
#include "psx/pad.h"

/* Sell-side sibling of world_menu_confirm_item_purchase.
 *
 * The category block reproduces the target's three loads of
 * g_world_shop_item_category: one for the reconcile argument, one `u8 cat`
 * shared by both `+ 1` operands (the target's a0, feeding `addiu a1,a0,1` and
 * `addiu v0,a0,1`), and a third feeding only the `!= 4` compare (the target's
 * v1). `u8` is load-bearing: an `s32 cat` lets cse fold the roles back
 * together.
 *
 * To keep the s8 truncation of `cat + 1` inside the arm rather than after the
 * join, `category` is the wide type (s32) with the narrowing spelled as a cast
 * on the value: `category = (s8)(cat + 1)`. In gcc 2.6.3 a local declared `s8`
 * is a bare SImode pseudo (expand_decl hands locals a promoted reg, not a
 * SUBREG_PROMOTED_VAR), so storing to it emits no conversion and the extend
 * is forced to the use. Widening the local makes the assignment itself the
 * conversion, which puts `addiu v0,a0,1 / sll v0,v0,0x18 / j /
 * sra a2,v0,0x18` in the arm; the else arm's `li a2,0x7` is already in range
 * and needs no extend. */
void world_menu_confirm_item_sale(void) {
    s32 result = 1;
    s32 category;
    s32 next_category;
    u8 cat;

    if (g_world_shop_fur_sale_message_wait != 0) {
        if (g_world_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_sale_message_wait = 0;
            world_thread_set_parameters(2, 0x19, 0xF841, 0);
            g_world_shop_menu_step = 0x12;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_item_sell_menu_2();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_obtain_gil(world_item_get_half_price(g_world_menu_entry_ids[g_world_menu_cursor_position])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_item_change_quantity_on_equip(g_world_menu_entry_ids[g_world_menu_cursor_position],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_world_menu_entry_ids[g_world_menu_cursor_position],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_thread_set_parameters(2, 0x19, 0xF837, 0);
            world_item_reconcile_sorted_list(g_world_shop_item_category);
            cat = g_world_shop_item_category;
            next_category = cat + 1;
            if (g_world_shop_item_category != 4) {
                category = (s8)(cat + 1);
            } else {
                category = 7;
            }
            /* An s32 third parameter is load-bearing: s8 re-narrows the value and sinks the extend past the join. */
            world_item_build_category_list(
                0, next_category, category, (world_item_list_entry_t*)g_world_menu_entry_ids, 0);
            world_menu_init_and_load_scrollable_list(g_world_menu_entry_ids, g_world_menu_cursor_position,
                g_world_text_item_names, g_world_shop_sale_confirm_menu_script);
            world_menu_set_window_scale_step(10);
        } else {
            world_thread_set_parameters(2, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_sale_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    world_menu_run_script_with_palette_mode(g_world_shop_sale_confirm_menu_script, 0, world_thread_is_running(1));
    world_shop_run_obtain_gil_menu();
}
