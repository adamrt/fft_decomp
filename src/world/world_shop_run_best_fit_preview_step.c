#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/menu_types.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

extern world_order_menu_entry_t g_world_formation_panel_windows[];

/*
 * Shop "best fit / fitting room" preview step, entry 8 of the shop callback
 * table at 0x8019525c.
 *
 * On entry it stages the selected unit's four weapon/armour slots, fills any
 * slot already taken from the shop with that slot's fitting-room reservation,
 * asks 0x80123dd8 for a suggested set, restores the staged slots and previews
 * the resulting stat delta. It then runs the confirmation menu; on confirm it
 * commits the suggested set slot by slot, returning each displaced item to the
 * fitting room or refunding a shop item's price, and returns to shop step 6.
 *
 * The gate at 0x801957a8 is `s8`, not `u8` (the target sign-loads it, `lb`).
 * Loop 2 computes both the suggested item and the unit's own slot at the
 * dominator; deferring either load into an arm costs a duplicate load and the
 * hoist of the selected-unit index.
 *
 * `item` is s32. As `s16` it is an HImode pseudo, so gcc emits a truncation
 * copy (`move s3,s0`) and stores through the copy while testing and passing
 * the original: seven callee-saved registers where the target uses six
 * (s0-s4 + ra), an extra save and a 136-byte frame. With `item` wide,
 * `shifted = suggested << 16; item = shifted >> 16;` restores the target's
 * `lhu` / `sll` / `sra` triple and lets the shifted value serve the sign test
 * (`bgez`); an HImode value folds the pair into one `lh`.
 *
 * The frame pad is split around the staging buffer -- `unused[16]`, `staged`,
 * `unused_tail[2]` -- to keep the buffer at the target's sp+80
 * (`.frame vars=88, regs=6/0, args=16`); a single `unused[18]` pushes it to
 * sp+88.
 *
 * The target has `move s1,v0` after world_menu_run_thread and no
 * initialisation of s1 at loop 2's head: loop 2's counter is the run_thread
 * result's allocno, so the source assigns the result to `i` and the loop is
 * written `for (; i < 4; i++)`. A fresh block-local takes a call-clobbered
 * register; reusing a function-scope allocno that is already dead puts the
 * counter in the target's callee-saved s1 and decides which of the two loop
 * quantities is allocated first.
 *
 * In the loop-1 preheader the target computes
 * `&g_world_formation_unit_pointers[idx]` first (into a1) and the
 * fitting-room row base second (into a0). Giving the unit slot its own local
 * (`unit_slot`), assigned just before `reserved`, and reading the loop body
 * through `(*unit_slot)->equipment[i]`, puts the two preheader address
 * computations in that order. The `lw` stays inside the loop, since the store
 * through `reserved` may alias the pointer array.
 *
 * Within `reserved`'s address expression the target materialises the
 * fitting-room base `lui`/`addiu` before the `idx*5*2` index arithmetic. cc1
 * itself emits `addu`/`sll`/`la`/`addu` in the other order under one `.loc`,
 * because the symbol canonicalises to the second operand of the address PLUS
 * and is expanded last; no operand respelling changes that (`fold` rewrites
 * `base - -(idx * 5)` straight back to a PLUS). Giving the row base its own
 * local and its own statement (`room_base = (u16*)&g_world_shop_fitting_room_items[0][0];`
 * then `reserved = room_base + idx * 5;`) forces the `la` to be emitted at
 * that statement, ahead of the index arithmetic.
 *
 * The target reloads `*choice` before storing 0 to
 * g_world_shop_best_fit_menu_open. `sched2` sinks that load below the store
 * from any source position, so the load goes into the `confirm` temp and an
 * empty `__asm__ volatile("")` between the load and the store holds it
 * there. A volatile read of `*choice` in place of the barrier does not.
 */
void world_shop_run_best_fit_preview_step(void) {
    /* The target reserves vars the code never touches: args 16 + vars 88 +
     * 6 saves 24 = the frame of 128, while only the 8-byte staging buffer at
     * sp+80 is accessed. */
    s32 unused[16];
    u16 staged[4];
    s32 unused_tail[2];
    s32 i;
    s32 item;
    s32 confirm;
    s16* choice;

    if (g_world_shop_best_fit_menu_open == 0) {
        world_formation_unit_t** unit_slot;
        u16* room_base;
        u16* reserved;

        g_world_shop_best_fit_menu.selected_index = 0;
        bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment, staged, 8);
        unit_slot = &g_world_formation_unit_pointers[g_world_formation_selected_unit_index];
        room_base = (u16*)&g_world_shop_fitting_room_items[0][0];
        reserved = room_base + g_world_formation_selected_unit_index * 5;
        for (i = 0; i < 4; i++, reserved++) {
            if (((*unit_slot)->equipment[i] >> 15) != 0) {
                (*unit_slot)->equipment[i] = *reserved;
            }
        }
        /* The callee's buffer is s16*; the cast reconciles the u16 equipment field and emits nothing. */
        world_shop_build_fitting_room_best_fit_equipment(
            g_world_formation_selected_unit_index, g_world_shop_id, (s16*)g_world_item_preview_stat_detail.equipment);
        g_world_item_preview_stat_detail.equipment[4]
            = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[4];
        bcopy(staged, g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment, 8);
        world_item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
            &g_world_selected_unit_stat_summary,
            g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
            g_world_item_preview_stat_detail.equipment);
        g_world_item_preview_stat_detail.two_hands
            = world_item_check_two_hands_for_weapons((struct weapon_pair*)g_world_item_preview_stat_detail.equipment,
                world_ability_has_two_hands(g_world_formation_selected_unit_index));
        world_menu_toggle_stat_preview_panel_thread(1);
        world_menu_toggle_preview_stats_window(1);
        g_world_formation_panel_windows[0].enabled = 1;
        g_world_preview_stats_thread_params.redraw_request = 1;
        g_world_shop_best_fit_menu_open = 1;
        g_world_formation_unit_cycle_mode = 1;
        g_world_menu_sound_muted = 1;
    }
    i = world_menu_run_thread(6, &g_world_shop_best_fit_menu);
    if (i == 0) {
        choice = &g_world_menu_selection_results[5];
        if (*choice == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_EQUIP;
        }
        if (*choice == 1) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        }
        world_menu_toggle_stat_preview_panel_thread(0);
        world_menu_toggle_preview_stats_window(0);
        g_world_formation_panel_windows[0].enabled = 1;
        confirm = *choice;
        /* Keeps the load above the store below (see the header). */
        __asm__ volatile("");
        g_world_shop_best_fit_menu_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        if (confirm == 0) {
            for (; i < 4; i++) {
                u16 suggested;
                s32 shifted;
                s32 current;

                /* The target reads the suggested item unsigned and widens it
                 * explicitly, reusing the shifted value for the sign test
                 * (`lhu` / `sll 16` / `sra 16`, then `bgez` on the shift), and
                 * it loads the unit's own slot ONCE at the loop head -- the
                 * `sra ,0xf` lands in the `bgez` delay slot and the same
                 * register is sign-extended again for world_item_get_price. */
                suggested = g_world_item_preview_stat_detail.equipment[i];
                shifted = suggested << 16;
                item = shifted >> 16;
                current = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[i];
                if (item == 0 || shifted < 0) {
                    world_shop_return_unit_equip_to_fitting_room(g_world_formation_selected_unit_index, (s16)i);
                    world_shop_add_fitting_room_cost(world_item_get_price(item));
                } else if ((current >> 15) != 0) {
                    world_shop_add_fitting_room_cost(-world_item_get_price((s16)current));
                    g_world_shop_fitting_room_items[g_world_formation_selected_unit_index][i] = 0;
                }
                g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[i] = item;
            }
            world_formation_recalculate_selected_unit_stats();
        }
        g_world_shop_menu_step = 6;
        g_world_menu_sound_muted = 0;
        world_thread_set_parameters(2, 0x19, -1, 0);
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}
