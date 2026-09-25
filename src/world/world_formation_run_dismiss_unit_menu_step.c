#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * The Soldier Office "dismiss unit" menu step. Three phases:
 *   - the dismissal animation timer (g_world_formation_dismiss_anim_timer) counting to 0x11, after
 *     which the roster slot is released and the script variables tied to the
 *     unit's sprite set are set;
 *   - first entry, which picks the prompt text and menu record; and
 *   - the per-frame poll of the prompt/menu thread.
 */

/*
 * Provisional. The sprite-set -> script-variable table walked by the
 * dismissal tail, terminated by sprite_set == -1. The four-byte stride is
 * proved by the target advancing both its record pointer and its byte-offset
 * index by 4 per iteration. Proposed for include/fft/world.h.
 */
typedef struct world_formation_dismiss_entry {
    s16 sprite_set;  /* 0x00 */
    s16 variable_id; /* 0x02 */
} world_formation_dismiss_entry_t;

/*
 * Provisional. The menu record handed to world_menu_run_thread for the
 * dismissal prompt. Offsets 0x74 is world_menu_thread_data_t.selection; the
 * other two fields are established here only. Proposed as an extension of
 * world_menu_thread_data_t in include/fft/world.h.
 */
typedef struct world_formation_dismiss_menu {
    u8 _pad00[0x74];
    s16 selection; /* 0x74 */
    u8 _pad76[0x94 - 0x76];
    s16 prompt_text_id; /* 0x94 */
    u8 _pad96[0xec - 0x96];
    s16 entry3_selected_index; /* 0xec: selected_index of the fourth 0x3c menu entry */
} world_formation_dismiss_menu_t;

extern world_formation_dismiss_entry_t g_world_formation_dismiss_entries[];
extern world_formation_dismiss_menu_t g_world_formation_dismiss_confirm_menu;
extern world_formation_dismiss_menu_t g_world_formation_dismiss_confirm_menu_scrolled;

void world_formation_run_dismiss_unit_menu_step(void) {
    world_formation_unit_t** units;
    /* The target keeps the unit-pointer base in s2 for the dismissal loop and
     * s0 for the first-entry block. One C variable is one pseudo, so those
     * are two variables. */
    world_formation_unit_t** units2;
    world_formation_unit_t* unit;
    world_formation_dismiss_entry_t* entry;
    world_formation_dismiss_menu_t* menu;
    s32 offset;
    /* Pinned: every assignment is an immediate, so the pin is free. It puts
     * the sentinel in v0, which leaves v1 for the guard load below and is
     * what the target does (li v0,-1 / lh v1,-0x2894(v1)). */
    register s32 sentinel __asm__("$2");
    s32 value;
    /* Split from `value`: the target holds the prompt text id in a1 and this
     * 0/1 flag in v0, so they cannot be one pseudo. */
    s32 flag;
    s32 sprite;
    s32 flags;
    s32 step;
    /* The sum `value + 0xE800` is its own variable: the target's a0 carries the
     * rand()/sprite accumulation, dies at `addu a1,a0,v0`, and a0 is then reused
     * for the 0x19/0x1a prompt kind. Non-overlapping roles in one register are
     * one variable, so the kind is carried in `value` and only the sum is split
     * out here. Pinning a separate `kind` to $4 reaches the same stores but
     * displaces `value` out of a0 across the whole rand() block; the split
     * alone, without the reuse, has no effect, because `combine_regs` ties the
     * sum to its dying summand. */
    s32 text_id;
    s32 selection;
    s8 running;

    if (g_world_formation_dismiss_anim_timer != 0) {
        /* Pre-increment, not a named load plus `+ 1`: the idiom forces the
         * expander to materialise the incremented value in its own pseudo,
         * which is what keeps the target's `move v1,v0` at +0x20 alive. With
         * a named temporary `cse` value-numbers both names into one pseudo
         * and deletes the copy; a two-variable tie, a self-tie launder or a
         * keepalive on the load still emits no move. */
        value = ++g_world_formation_dismiss_anim_timer;
        if ((value & 0xFF) < 0x11) {
            return;
        }
        /* The sentinel is set twice so loop.c sees it vary and leaves it in
         * the body, where the target rematerialises it into the `lh` shadow
         * rather than paying a fourth callee-saved register for the hoist. */
        sentinel = -1;
        offset = 0;
        if (g_world_formation_dismiss_entries[0].sprite_set != sentinel) {
            units = g_world_formation_unit_pointers;
            /* Two-variable tie (output `entry`, input the array symbol): keeps
             * the guard load and this walker from sharing one base register.
             * Without it cc1 emits `la $4,SYM; lh $2,0($4)` then `move s0,$4`;
             * the target folds %lo into the guard `lh` and materialises the
             * address again in s0. */
            __asm__("" : "=r"(entry) : "0"(g_world_formation_dismiss_entries));
            do {
                if (units[g_world_formation_selected_unit_index]->sprite_set == entry->sprite_set) {
                    /* Read through the separate byte-offset index, not entry (see the table note). */
                    world_script_set_variable(
                        *(s16*)((u8*)&g_world_formation_dismiss_entries[0].variable_id + offset), 0xC);
                }
                entry++;
                offset += 4;
                sentinel = -1;
            } while (entry->sprite_set != sentinel);
        }
        world_formation_dismiss_unit(g_world_formation_selected_unit_index);
        g_world_menu_sound_muted = 0;
        g_world_formation_current_menu = 0;
        return;
    }

    if (*(s8*)&g_world_dismiss_unit_prompt_running == 0) {
        sprite = g_world_formation_selected_unit_index;
        units2 = g_world_formation_unit_pointers;
        if (g_world_formation_unit_pointers[sprite]->proposition_status != 0) {
            world_text_show_message_and_play_sound(0xC012, 0x30);
            g_world_formation_dismiss_step = 2;
            flag = 1;
        } else {
            g_world_formation_dismiss_step = world_formation_can_dismiss_unit((s16)sprite);
            world_gfx_bind_data_pointer(4);
            if (g_world_formation_unit_browse_enabled != 0) {
                g_world_formation_unit_cycle_mode = 1;
            }
            g_world_text_substitution_values[0] = units2[g_world_formation_selected_unit_index]->roster_slot;
            unit = units2[g_world_formation_selected_unit_index];
            sprite = unit->sprite_set;
            if (sprite >= 0x4B) {
                flags = unit->gender_flags;
                if (flags & 0x80) {
                    value = rand() % 8;
                } else if (flags & 0x40) {
                    value = rand() % 8 + 8;
                } else {
                    value = rand() % 8 + 0x10;
                }
            } else {
                if (sprite == 0x48) {
                    sprite = 0x49;
                }
                if (g_world_formation_dismiss_step != 0) {
                    value = sprite + 0x17;
                } else {
                    value = sprite + 0x61;
                }
            }
            text_id = value + 0xE800;
            if (g_world_formation_dismiss_step == 0) {
                g_world_menu_sound_muted = 1;
                g_world_formation_dismiss_menu_data = g_world_formation_dismiss_prompt;
                if (g_world_formation_scroll_enabled != 0) {
                    value = 0x19;
                } else {
                    value = 0x1A;
                }
                g_world_formation_dismiss_prompt_kind = value;
                g_world_formation_dismiss_prompt_text_id = text_id;
            } else {
                menu = &g_world_formation_dismiss_confirm_menu;
                if (g_world_formation_scroll_enabled != 0) {
                    menu = &g_world_formation_dismiss_confirm_menu_scrolled;
                }
                g_world_formation_dismiss_menu_data = (u8*)menu;
                menu->selection = 0;
                menu->entry3_selected_index = 0;
                menu->prompt_text_id = text_id;
            }
            flag = 1;
        }
        *(s8*)&g_world_dismiss_unit_prompt_running = flag;
        g_world_menu_description_text_id = -1;
    }

    if (g_world_formation_dismiss_step == 2) {
        running = world_thread_is_running(1);
        *(s8*)&g_world_dismiss_unit_prompt_running = running;
        if (running != 0) {
            return;
        }
        g_world_formation_current_menu = 0;
        return;
    }

    running = world_menu_run_thread(6, g_world_formation_dismiss_menu_data);
    step = g_world_formation_dismiss_step;
    *(s8*)&g_world_dismiss_unit_prompt_running = running;
    if (step != 0) {
        if (world_thread_is_running(3) != 0) {
            step = g_world_formation_dismiss_step;
            if (step == 1) {
                selection = g_world_menu_selection_results[10];
                g_world_menu_sound_muted = step;
                if (selection == 0) {
                    g_world_menu_sound_effect_id = MAIN_SFX_DISMISS;
                } else if (selection == step) {
                    g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                }
                /* A default assignment plus a conditional override, not an
                 * if/else over the two stores: the target puts `li v1,1` in the
                 * branch's delay slot and overrides with 0x64 on the
                 * fall-through (`beqz` at 0x3e4). An if/else emits the opposite
                 * branch sense and orders the two `li`s the other way, which
                 * also costs the store's register. */
                step = 1;
                if (g_world_menu_sound_effect_id != 0) {
                    step = 0x64;
                }
                g_world_formation_dismiss_step = step;
            }
        }
        if (*(s8*)&g_world_dismiss_unit_prompt_running != 0) {
            return;
        }
        if (g_world_menu_selection_results[10] != 0) {
            g_world_menu_sound_muted = 0;
            g_world_formation_current_menu = 0;
            return;
        }
        g_world_formation_dismiss_anim_timer = (*(u16*)&g_world_formation_selected_unit_index << 8) + 1;
        return;
    }
    if (running != 0) {
        return;
    }
    g_world_menu_sound_muted = 0;
    g_world_formation_current_menu = 0;
}
