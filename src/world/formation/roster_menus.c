/* Formation lets the player reorder units and dismiss a unit after its farewell prompt. */

#include "fft/formation.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/message_box.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "psx/libc.h"
#include "psx/types.h"

extern menu_text_binding_t g_formation_selected_unit_text_binding;
extern menu_entry_t g_formation_dismiss_prompt;
extern menu_entry_t g_formation_dismiss_confirm_menu[4];
extern menu_entry_t g_formation_dismiss_confirm_menu_scrolled[4];
extern menu_entry_t* g_formation_dismiss_menu_data;
extern u8 g_formation_dismiss_step;
extern menu_entry_t g_formation_order_menu;
extern u8 g_formation_order_saved_browse_enabled;
extern s16 g_formation_order_entries[];

/* The dismiss prompt keeps its eligibility result until an answer queues a sound. */
enum {
    FORMATION_DISMISS_REFUSED = 0,
    FORMATION_DISMISS_CONFIRM = 1,
    FORMATION_DISMISS_PROPOSITION_MESSAGE = 2,
    FORMATION_DISMISS_SOUND_QUEUED = 100
};

/* Special identities whose Brave Story profile is marked when dismissed. */
typedef struct dismiss_profile {
    s16 character_identity;
    s16 variable_id; /* EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST range */
} dismiss_profile_t;

static dismiss_profile_t g_dismiss_profiles[] = { { CHARACTER_IDENTITY_AGRIAS_JOIN, 0x3ce },
    { CHARACTER_IDENTITY_AGRIAS_GUEST, 0x3ce }, { CHARACTER_IDENTITY_ORLANDU, 0x3cf },
    { CHARACTER_IDENTITY_BEOWULF, 0x3d6 }, { CHARACTER_IDENTITY_REIS_HUMAN, 0x3d8 },
    { CHARACTER_IDENTITY_REIS_DRAGON, 0x3d8 }, { CHARACTER_IDENTITY_RAFA_JOIN, 0x3dc },
    { CHARACTER_IDENTITY_MALAK, 0x3dd }, { CHARACTER_IDENTITY_MUSTADIO_JOIN, 0x3e2 },
    { CHARACTER_IDENTITY_MUSTADIO_GUEST, 0x3e2 }, { CHARACTER_IDENTITY_BALMAFULA, 0x3d9 },
    { CHARACTER_IDENTITY_MELIADOUL_JOIN, 0x3ed }, { CHARACTER_IDENTITY_CLOUD, 0x3ee }, { -1, 0 } };

/*
 * Per-frame handler of the formation screen's "order" (sort) menu.
 *
 * The menu lists the seven sort keys in their current priority order.
 * Choosing a row moves that key to the front, shifting the rows above it
 * down one, stores the order back and re-sorts the roster. While the menu is
 * open the unit banner, unit browsing and the two detail panels are off; the
 * handler returns to the unit grid when the menu thread ends.
 */
void formation_order_menu_update(void) {
    s16 option_ids[12]; /* seven entries plus terminator; sized for the retail frame */
    s32 index;
    s32 selected_entry;

    if (g_formation_order_open == 0) {
        item_load_type_order(0, g_formation_order_entries);
        for (index = 0; index < FORMATION_SORT_KEY_COUNT; index++) {
            option_ids[index] = g_formation_order_entries[index] + 14;
        }
        option_ids[FORMATION_SORT_KEY_COUNT] = -1;
        text_concatenate_entries(g_formation_menu_text_table, g_formation_menu_text_buffer, option_ids, 1);
        g_formation_unit_banner_enabled = 0;
        g_formation_order_saved_browse_enabled = g_formation_unit_browse_enabled;
        g_formation_unit_browse_enabled = 0;
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
        g_formation_order_menu.selected_index = 0;
        g_formation_order_open = 1;
    }

    if (menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, &g_formation_order_menu) == 0) {
        g_formation_order_open = 0;
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        g_formation_unit_browse_enabled = g_formation_order_saved_browse_enabled;
    }

    g_help_message_text_id = g_formation_order_entries[g_formation_order_menu.selected_index] + 0x103f;
    if (g_screen_selection_results[9] != -1) {
        /* Move the chosen entry to the front and redraw the options. */
        index = g_screen_selection_results[9];
        selected_entry = g_formation_order_entries[index];
        index -= 1;
        for (; index >= 0; index--) {
            g_formation_order_entries[index + 1] = g_formation_order_entries[index];
        }
        g_formation_order_entries[0] = selected_entry;
        screen_selection_results_reset();
        for (index = 0; index < FORMATION_SORT_KEY_COUNT; index++) {
            option_ids[index] = g_formation_order_entries[index] + 14;
        }
        option_ids[FORMATION_SORT_KEY_COUNT] = -1;
        text_concatenate_entries(g_formation_menu_text_table, g_formation_menu_text_buffer, option_ids, 1);
        g_screen_text_redraw_request = 1;
        item_store_type_order(0, g_formation_order_entries);
        formation_roster_sort(0);
    }
}

/* Message entry thread whose speaker is the selected formation unit's
 * roster slot. */
void formation_unit_message_thread(void) {
    /* The retail code reads roster_slot with lhu; keep the byte-exact width. */
    g_formation_selected_unit_text_binding.speaker_id
        = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;
    menu_text_entry_wait_thread();
}

/*
 * Per-frame step of the "dismiss unit" menu
 * (FORMATION_MENU_DISMISS_UNIT), called by formation_grid_menu_update.
 * Three phases: first entry, which picks the prompt text and menu record;
 * the per-frame poll of the prompt or menu thread; and the dismissal timer.
 *
 * On entry a unit that is away on a proposition only gets message 0xC012.
 * Otherwise g_formation_dismiss_step takes
 * formation_can_dismiss's answer: 0 shows the unit's refusal line
 * and returns to the grid, nonzero runs the yes/no confirmation. The line
 * comes from the dismiss-reply text (TEXT_ID_FORMATION_TEXT_BASE + n): identities from Girl
 * upward draw one of eight lines by gender or monster kind. Earlier identities, including Boy,
 * use their identity + 0x17 for the farewell and + 0x61 for refusal. Dragon Reis uses the
 * following identity's text slots.
 *
 * Confirming plays the dismiss sound and starts a 16-frame timer (the unit
 * index is carried in its high byte); when it runs out every
 * g_dismiss_profiles row matching the unit's sprite set has
 * its script variable set to 0xC and the unit is removed.
 */
void formation_dismiss_menu_update(void) {
    formation_unit_t* unit;
    menu_entry_t* menu;
    s32 i;
    s32 value;
    s32 sprite;
    s32 flags;
    s32 step;
    /* The sum `value + TEXT_ID_FORMATION_TEXT_BASE` is its own variable: the target's a0 carries the
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

    /* The timer's low byte counts the 16 frames; its high byte only carries the unit index. */
    if (g_formation_dismiss_anim_timer != 0) {
        g_formation_dismiss_anim_timer++;
        if ((g_formation_dismiss_anim_timer & 0xFF) < 0x11) {
            return;
        }
        for (i = 0; g_dismiss_profiles[i].character_identity != -1; i++) {
            if (g_formation_unit_pointers[g_unit_selected_index]->sprite_set
                == g_dismiss_profiles[i].character_identity) {
                script_variable_set(g_dismiss_profiles[i].variable_id, 0xC);
            }
        }
        formation_dismiss(g_unit_selected_index);
        g_sound_suppressed = 0;
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        return;
    }

    if (g_formation_dismiss_prompt_running == 0) {
        sprite = g_unit_selected_index;
        if (g_formation_unit_pointers[sprite]->proposition_status != 0) {
            message_box_show_with_sound(0xC012, SOUND_SFX_ERROR_MESSAGE);
            g_formation_dismiss_step = FORMATION_DISMISS_PROPOSITION_MESSAGE;
            g_formation_dismiss_prompt_running = 1;
        } else {
            g_formation_dismiss_step = formation_can_dismiss(sprite);
            screen_text_section_bind(SCREEN_TEXT_DATA_DISMISS_UNIT_REPLIES);
            if (g_formation_unit_browse_enabled != 0) {
                g_formation_unit_cycle_mode = 1;
            }
            g_text_substitution_values[0] = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;

            /* The random-reply range begins at Girl; Boy still takes the indexed reply path. */
            unit = g_formation_unit_pointers[g_unit_selected_index];
            sprite = unit->sprite_set;
            if (sprite >= CHARACTER_IDENTITY_GIRL) {
                flags = unit->gender_flags;
                if (flags & UNIT_FLAG_MALE) {
                    value = rand() % 8;
                } else if (flags & UNIT_FLAG_FEMALE) {
                    value = rand() % 8 + 8;
                } else {
                    value = rand() % 8 + 0x10;
                }
            } else {
                if (sprite == CHARACTER_IDENTITY_REIS_DRAGON) {
                    sprite = CHARACTER_IDENTITY_REIS_DRAGON + 1;
                }
                if (g_formation_dismiss_step != FORMATION_DISMISS_REFUSED) {
                    value = sprite + 0x17;
                } else {
                    value = sprite + 0x61;
                }
            }
            text_id = value + TEXT_ID_FORMATION_TEXT_BASE;

            /* A refusal is a plain prompt; a dismissable unit gets the yes/no menu with its farewell. */
            if (g_formation_dismiss_step == FORMATION_DISMISS_REFUSED) {
                g_sound_suppressed = 1;
                g_formation_dismiss_menu_data = &g_formation_dismiss_prompt;
                if (g_formation_unit_banner_at_bottom != 0) {
                    value = 0x19;
                } else {
                    value = 0x1A;
                }
                g_formation_dismiss_prompt.header_id = value;
                g_formation_dismiss_prompt.text_id = text_id;
            } else {
                menu = g_formation_dismiss_confirm_menu;
                if (g_formation_unit_banner_at_bottom != 0) {
                    menu = g_formation_dismiss_confirm_menu_scrolled;
                }
                g_formation_dismiss_menu_data = menu;
                menu[1].selected_index = 0;
                menu[3].selected_index = 0;
                menu[2].text_id = text_id;
            }
            g_formation_dismiss_prompt_running = 1;
        }
        g_help_message_text_id = -1;
    }

    if (g_formation_dismiss_step == FORMATION_DISMISS_PROPOSITION_MESSAGE) {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        g_formation_dismiss_prompt_running = running;
        if (running != 0) {
            return;
        }
        g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        return;
    }

    running = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, g_formation_dismiss_menu_data);
    step = g_formation_dismiss_step;
    g_formation_dismiss_prompt_running = running;
    if (step != FORMATION_DISMISS_REFUSED) {
        /* Step 1 queues the answer's sound once; 0x64 then keeps the step nonzero without repeating it. */
        if (thread_is_running(3) != 0) {
            step = g_formation_dismiss_step;
            if (step == FORMATION_DISMISS_CONFIRM) {
                selection = g_screen_selection_results[10];
                g_sound_suppressed = step;
                if (selection == 0) {
                    g_sound_queued_effect_id = SOUND_SFX_DISMISS;
                } else if (selection == step) {
                    g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                }
                /* A default assignment plus a conditional override, not an
                 * if/else over the two stores: the target puts `li v1,1` in the
                 * branch's delay slot and overrides with 0x64 on the
                 * fall-through (`beqz` at 0x3e4). An if/else emits the opposite
                 * branch sense and orders the two `li`s the other way, which
                 * also costs the store's register. */
                step = FORMATION_DISMISS_CONFIRM;
                if (g_sound_queued_effect_id != SOUND_SFX_NONE) {
                    step = FORMATION_DISMISS_SOUND_QUEUED;
                }
                g_formation_dismiss_step = step;
            }
        }

        if (g_formation_dismiss_prompt_running != 0) {
            return;
        }
        if (g_screen_selection_results[10] != 0) {
            g_sound_suppressed = 0;
            g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
            return;
        }
        g_formation_dismiss_anim_timer = (g_unit_selected_index << 8) + 1;
        return;
    }

    if (running != 0) {
        return;
    }
    g_sound_suppressed = 0;
    g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
}
