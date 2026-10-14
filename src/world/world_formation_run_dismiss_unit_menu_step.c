#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Per-frame step of the "dismiss unit" menu
 * (WORLD_FORMATION_MENU_DISMISS_UNIT), called by world_formation_process_frame.
 * Three phases: first entry, which picks the prompt text and menu record;
 * the per-frame poll of the prompt or menu thread; and the dismissal timer.
 *
 * On entry a unit that is away on a proposition only gets message 0xC012.
 * Otherwise g_world_formation_dismiss_step takes
 * world_formation_can_dismiss_unit's answer: 0 shows the unit's refusal line
 * and returns to the grid, nonzero runs the yes/no confirmation. The line
 * comes from the dismiss-reply text (0xE800 + n): a generic unit (sprite set
 * 0x4B and up) draws one of eight lines for its gender or monster kind with
 * rand(); a special unit uses its sprite set, + 0x17 for the farewell and
 * + 0x61 for the refusal, with sprite set 0x48 sharing 0x49's lines.
 *
 * Confirming plays the dismiss sound and starts a 16-frame timer (the unit
 * index is carried in its high byte); when it runs out every
 * g_world_formation_dismiss_entries row matching the unit's sprite set has
 * its script variable set to 0xC and the unit is removed.
 */
void world_formation_run_dismiss_unit_menu_step(void) {
    world_unit_t* unit;
    menu_entry_t* menu;
    s32 i;
    s32 value;
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
        g_world_formation_dismiss_anim_timer++;
        if ((g_world_formation_dismiss_anim_timer & 0xFF) < 0x11) {
            return;
        }
        for (i = 0; g_world_formation_dismiss_entries[i].sprite_set != -1; i++) {
            if (g_world_unit_pointers[g_world_formation_selected_unit_index]->sprite_set
                == g_world_formation_dismiss_entries[i].sprite_set) {
                script_set_variable(g_world_formation_dismiss_entries[i].variable_id, 0xC);
            }
        }
        world_formation_dismiss_unit(g_world_formation_selected_unit_index);
        g_sound_suppressed = 0;
        g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        return;
    }

    if (g_world_dismiss_unit_prompt_running == 0) {
        sprite = g_world_formation_selected_unit_index;
        if (g_world_unit_pointers[sprite]->proposition_status != 0) {
            world_text_show_message_and_play_sound(0xC012, MAIN_SFX_ERROR_MESSAGE);
            g_world_formation_dismiss_step = 2;
            g_world_dismiss_unit_prompt_running = 1;
        } else {
            g_world_formation_dismiss_step = world_formation_can_dismiss_unit(sprite);
            world_text_bind_section_data(WORLD_TEXT_DATA_DISMISS_UNIT_REPLIES);
            if (g_world_formation_unit_browse_enabled != 0) {
                g_world_formation_unit_cycle_mode = 1;
            }
            g_text_substitution_values[0]
                = g_world_unit_pointers[g_world_formation_selected_unit_index]->identity.roster_slot;
            unit = g_world_unit_pointers[g_world_formation_selected_unit_index];
            sprite = unit->sprite_set;
            if (sprite >= 0x4B) {
                flags = unit->gender_flags;
                if (flags & UNIT_FLAG_MALE) {
                    value = rand() % 8;
                } else if (flags & UNIT_FLAG_FEMALE) {
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
                g_sound_suppressed = 1;
                g_world_formation_dismiss_menu_data = &g_world_formation_dismiss_prompt;
                if (g_world_formation_unit_banner_at_bottom != 0) {
                    value = 0x19;
                } else {
                    value = 0x1A;
                }
                g_world_formation_dismiss_prompt_kind = value;
                g_world_formation_dismiss_prompt_text_id = text_id;
            } else {
                menu = g_world_formation_dismiss_confirm_menu;
                if (g_world_formation_unit_banner_at_bottom != 0) {
                    menu = g_world_formation_dismiss_confirm_menu_scrolled;
                }
                g_world_formation_dismiss_menu_data = menu;
                menu[1].selected_index = 0;
                menu[3].selected_index = 0;
                menu[2].text_id = text_id;
            }
            g_world_dismiss_unit_prompt_running = 1;
        }
        g_world_menu_description_text_id = -1;
    }

    if (g_world_formation_dismiss_step == 2) {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        g_world_dismiss_unit_prompt_running = running;
        if (running != 0) {
            return;
        }
        g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        return;
    }

    running = world_menu_run_thread(THREAD_SCREEN_SLOT_MENU, g_world_formation_dismiss_menu_data);
    step = g_world_formation_dismiss_step;
    g_world_dismiss_unit_prompt_running = running;
    if (step != 0) {
        if (thread_is_running(3) != 0) {
            step = g_world_formation_dismiss_step;
            if (step == 1) {
                selection = g_world_menu_selection_results[10];
                g_sound_suppressed = step;
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
                if (g_world_menu_sound_effect_id != MAIN_SFX_NONE) {
                    step = 0x64;
                }
                g_world_formation_dismiss_step = step;
            }
        }
        if (g_world_dismiss_unit_prompt_running != 0) {
            return;
        }
        if (g_world_menu_selection_results[10] != 0) {
            g_sound_suppressed = 0;
            g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
            return;
        }
        g_world_formation_dismiss_anim_timer = (g_world_formation_selected_unit_index << 8) + 1;
        return;
    }
    if (running != 0) {
        return;
    }
    g_sound_suppressed = 0;
    g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
}
