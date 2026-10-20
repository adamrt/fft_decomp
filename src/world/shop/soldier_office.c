/* The Soldier Office previews recruits, collects hire fees and renames existing party members. */

#include "fft/formation.h"
#include "fft/help.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/message_box.h"
#include "fft/name_entry.h"
#include "fft/party.h"
#include "fft/screen.h"
#include "fft/shop.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_shop_background_visible;

/* The Soldier Office's party count and limit display stream, through its end opcode. */
typedef struct unit_count_script {
    menu_draw_state_command_t priority;                 /* 0x00 */
    menu_texture_page_command_t texture_page;           /* 0x04 */
    menu_fixed_clut_command_t clut;                     /* 0x09 */
    menu_sprite_command_t sprites[2];                   /* 0x0e */
    menu_draw_number_command_t unit_count;              /* 0x20 */
    menu_draw_number_command_t unit_limit;              /* 0x2a; direct value 16 */
    menu_texture_page_command_t separator_texture_page; /* 0x34 */
    menu_sprite_command_t separator;                    /* 0x39 */
    u8 end;                                             /* 0x42; SCREEN_SCRIPT_END */
} unit_count_script_t;
typedef char unit_count_script_size_must_be_67[(sizeof(unit_count_script_t) == 67) ? 1 : -1];
extern u8 g_soldier_office_hire_menu_initialized;
extern u8 g_soldier_office_hire_unread_flag_1; /* cleared when the Soldier Office hire menu opens; never read */

extern u8 g_soldier_office_hire_unread_flag_2; /* as g_soldier_office_hire_unread_flag_1 */

extern menu_entry_t g_soldier_office_hire_confirm_menu;
extern u8 g_soldier_office_hire_confirm_message_wait;
extern menu_entry_t g_soldier_office_hire_fee_confirm_menu;
extern u8 g_soldier_office_hire_fee_message_wait;
extern u8 g_soldier_office_hire_name_entry_initialized;
extern u8 g_soldier_office_hire_result_shown;
extern u8 g_soldier_office_hire_preview_initialized;
extern u8 g_soldier_office_rename_prompt_initialized;
extern u8 g_soldier_office_rename_name_entry_initialized;
extern u8 g_soldier_office_rename_error_shown;
extern const u8* g_soldier_office_entered_name;
extern u32 g_soldier_office_fee;
extern unit_count_script_t g_soldier_office_unit_count_script;
extern u8 g_soldier_office_unit_count;

/* The cursor doubles as the soldier-office mode (hire_fee_get). */
extern menu_entry_t g_soldier_office_hire_menu_thread_data;
extern u8 g_soldier_office_fee_script[34];

/* Soldier Office main step: runs the hire menu. Rows 0-1 generate a recruit of the chosen
 * kind, whose fee is 1000 gil plus its starting equipment's price (when the party cannot pay,
 * the recruit is removed again); row 2 renames a monster; row 3 or a cancel leaves, after
 * cashing out inventory beyond 99 of an item. */
void soldier_office_menu_update(void) {
    unit_stats_t unit;
    s32 i;
    s32 result;
    s32 slot;

    if (g_soldier_office_hire_menu_initialized == 0) {
        g_soldier_office_hire_confirm_menu.selected_index = 0;
        g_soldier_office_hire_fee_confirm_menu.selected_index = 0;
        g_soldier_office_hire_result_shown = 0;
        g_soldier_office_hire_menu_initialized = 1;
        g_unit_panel_status_display_thread_params.position.y = 0;
        g_soldier_office_hire_unread_flag_1 = 0;
        g_soldier_office_hire_unread_flag_2 = 0;
        g_formation_unit_banner_enabled = 0;
        g_soldier_office_unit_count = 0;
        for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
            result = unit_party_slot_init(&unit, i, 1);
            if (result == 0 && unit.formation_index < PARTY_GUEST_SLOT_FIRST) {
                g_soldier_office_unit_count++;
            }
        }
    }
    if (g_soldier_office_hire_result_shown != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    } else {
        i = menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, &g_soldier_office_hire_menu_thread_data);
        if (i == 0) {
            if (g_screen_selection_results[0] == 3 || g_screen_selection_results[0] == -1) {
                g_soldier_office_hire_menu_initialized = 0;
                g_shop_menu_step = SHOP_STEP_EXIT_FADE;
                thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF825, 0);
                shop_cash_out_excess_items();
            } else if (g_screen_selection_results[0] == 2) {
                g_formation_unit_count
                    = formation_record_list_build(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_MONSTERS);
                if (g_formation_unit_count != 0) {
                    g_shop_menu_step = SHOP_STEP_RENAME_SELECT_UNIT;
                    i = 0xF847;
                } else {
                    i = 0xF846;
                    g_soldier_office_hire_result_shown = 1;
                }
                thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, i, 0);
            } else {
                slot = formation_generate_unit(g_screen_selection_results[0]);
                if (slot >= 0) {
                    g_soldier_office_fee = 1000;
                    /* `i` is still 0, the menu thread's result. */
                    for (; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                        g_soldier_office_fee
                            += shop_item_price_get(g_formation_unit_pointers[slot]->detail.equipment[i]);
                    }
                    if (shop_gil_adjust(0) < g_soldier_office_fee) {
                        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF823, 0);
                        party_unit_remove(g_formation_unit_pointers[slot]->identity.roster_slot);
                        g_soldier_office_hire_result_shown = 1;
                    } else {
                        g_unit_selected_index = slot;
                        formation_stage_selected_unit();
                        g_formation_unit_banner_at_bottom = 0;
                        g_formation_unit_banner_at_bottom_latch = 0;
                        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF824, 0);
                        g_shop_menu_step = SHOP_STEP_HIRE_PREVIEW;
                    }
                } else {
                    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF822, 0);
                    g_soldier_office_hire_result_shown = 1;
                }
            }
        }
    }
    shop_gil_draw();
    soldier_office_fee_draw();
    soldier_office_unit_count_draw();
}

/*
 * Soldier office step that shows the generated recruit.
 *
 * Any face button dismisses the message box; once it has closed, unit
 * browsing and the status banner are turned on for the preview. A further
 * face button closes the panels and moves on to the hire confirmation
 * (message 0xF826).
 */
void soldier_office_hire_preview_update(void) {
    if (g_soldier_office_hire_preview_initialized == 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_formation_unit_browse_enabled = 1;
            g_formation_unit_banner_enabled = 1;
            g_formation_unit_banner_at_bottom = 0;
            g_unit_panel_status_display_thread_params.position.y = 0;
            g_help_message_text_id = MENU_GRID_HELP_RECRUIT_PREVIEW;
            g_soldier_office_hire_preview_initialized = 1;
        }
        shop_gil_draw();
        soldier_office_fee_draw();
        soldier_office_unit_count_draw();
    } else if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        g_shop_menu_step = SHOP_STEP_HIRE_CONFIRM;
        g_formation_unit_browse_enabled = 0;
        g_formation_unit_banner_enabled = 0;
        unit_panel_toggle_numbers_thread(0);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF826, 0);
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        g_soldier_office_hire_preview_initialized = 0;
    }
}

/*
 * Soldier office: confirm hiring the previewed recruit.
 *
 * Selection 1 or -1 declines and removes the recruit from the roster;
 * selection 2 returns to the preview. Any other result shows a message and
 * waits for its thread before going on to the name entry.
 */
void soldier_office_hire_confirm_update(void) {
    s16 selection;

    if (g_soldier_office_hire_confirm_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_soldier_office_hire_confirm_message_wait = 0;
            g_shop_menu_step = SHOP_STEP_HIRE_NAME_ENTRY;
        }
    } else if (menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, &g_soldier_office_hire_confirm_menu) == 0) {
        selection = g_screen_selection_results[2];
        if (selection == 1 || selection == -1) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF827, 0);
            g_soldier_office_hire_menu_initialized = 0;
            party_unit_remove(g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_MENU;
        } else if (selection == 2) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_shop_menu_step = SHOP_STEP_HIRE_PREVIEW;
        } else {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF829, 0);
            g_soldier_office_hire_confirm_message_wait = 1;
        }
    }
    shop_gil_draw();
    soldier_office_fee_draw();
    soldier_office_unit_count_draw();
}

/* Soldier office: the name entry keyboard for the recruit, then the fee prompt. */
void soldier_office_hire_name_update(void) {
    if (g_soldier_office_hire_name_entry_initialized == 0) {
        g_formation_unit_banner_enabled = 0;
        unit_panel_toggle_banner_thread(0);
        g_soldier_office_hire_name_entry_initialized = 1;
    }
    g_soldier_office_entered_name
        = name_entry_keyboard_update(g_formation_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_soldier_office_entered_name != 0) {
        g_soldier_office_hire_name_entry_initialized = 0;
        g_text_substitution_values[0] = g_soldier_office_fee;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82A, 0);
        g_shop_menu_step = SHOP_STEP_HIRE_FEE_CONFIRM;
    }
}

/*
 * Soldier office: confirm the hiring fee.
 *
 * Choice 0 pays the fee and stores the recruit's name; 1 or -1 removes the
 * recruit from the roster. Other choices return to the name entry.
 */
void soldier_office_hire_fee_update(void) {
    s16 choice;

    g_help_message_text_id = 0;
    if (g_soldier_office_hire_fee_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
            g_soldier_office_hire_fee_message_wait = 0;
        }
    } else if (menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, &g_soldier_office_hire_fee_confirm_menu) == 0) {
        choice = g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT];
        if (choice == 0) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf82b, 0);
            shop_gil_adjust(-g_soldier_office_fee);
            formation_unit_name_set(g_unit_selected_index, g_soldier_office_entered_name);
            g_soldier_office_hire_fee_message_wait = 1;
        } else if (choice == 1 || choice == -1) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf827, 0);
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_MENU;
            party_unit_remove(g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_soldier_office_hire_menu_initialized = 0;
        } else {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_soldier_office_hire_fee_confirm_menu.selected_index = 0;
            g_shop_menu_step = SHOP_STEP_HIRE_NAME_ENTRY;
        }
    }
    shop_gil_draw();
    soldier_office_fee_draw();
    soldier_office_unit_count_draw();
}

/* Return to the soldier office menu with message 0xf821, redrawing the gil,
 * fee and roster-count displays for this frame. */
void soldier_office_menu_reopen(void) {
    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf821, 0);
    g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_MENU;
    g_soldier_office_hire_menu_initialized = 0;
    shop_gil_draw();
    soldier_office_fee_draw();
    soldier_office_unit_count_draw();
}

/* Menu callback: no marker (-1) for any unit. */
static s32 rename_unit_marker_get(s32 unused_unit_index) {
    return -1;
}

/*
 * Soldier Office rename step: waits for a face button to dismiss the
 * message, then shows the monster grid. Circle opens the name entry for the
 * selected unit, except that Reis in dragon form refuses (message 0xF848, in
 * a box placed away from the unit banner); Cross returns to the Soldier
 * Office menu.
 */
void soldier_office_rename_unit_update(void) {
    s32 mode;
    s32 input;
    s32 text_id;

    if (g_soldier_office_rename_prompt_initialized == 0) {
        if (!(g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE))) {
            shop_gil_draw();
            soldier_office_fee_draw();
            soldier_office_unit_count_draw();
            return;
        }
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_soldier_office_rename_error_shown = 0;
        g_soldier_office_rename_prompt_initialized = 1;
        formation_grid_cursor_init(0);
        g_shop_background_visible = 0;
    }
    if (g_formation_unit_banner_enabled != 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_help_message_text_id = text_id;
    } else {
        g_help_message_text_id = 0;
    }
    formation_grid_update(1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode, rename_unit_marker_get, 0);
    if (g_soldier_office_rename_error_shown != 0) {
        g_help_message_text_id = -1;
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_soldier_office_rename_error_shown = 0;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0 && formation_grid_scroll_check() == 0) {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CIRCLE) {
            if (g_formation_unit_pointers[g_unit_selected_index]->sprite_set == CHARACTER_IDENTITY_REIS_DRAGON) {
                if (g_formation_unit_banner_at_bottom != 0) {
                    mode = 1;
                } else {
                    mode = 2;
                }
                thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
                thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, mode | 0x18, 0xF848, 0);
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
                g_soldier_office_rename_error_shown = 1;
            } else {
                g_shop_menu_step = SHOP_STEP_RENAME_NAME_ENTRY;
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                g_formation_unit_banner_enabled = 0;
                g_soldier_office_rename_prompt_initialized = 0;
            }
        } else if (input & PSX_PAD_CROSS) {
            g_shop_background_visible = 1;
            g_soldier_office_rename_prompt_initialized = 0;
            g_formation_unit_banner_enabled = 0;
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    }
}

/* Soldier office: the name entry keyboard of the Rename service; sibling of
 * soldier_office_hire_name_update, which goes on to the fee prompt (0xF82A). */
void soldier_office_rename_name_update(void) {
    if (g_soldier_office_rename_name_entry_initialized == 0) {
        g_formation_unit_banner_enabled = 0;
        unit_panel_toggle_banner_thread(0);
        g_soldier_office_rename_name_entry_initialized = 1;
    }
    formation_grid_update(1, 0, 0, 0, g_menu_input_lock_mode, rename_unit_marker_get, 0);
    g_soldier_office_entered_name
        = name_entry_keyboard_update(g_formation_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_soldier_office_entered_name != 0) {
        g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        formation_unit_name_set(g_unit_selected_index, g_soldier_office_entered_name);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
        g_shop_background_visible = 1;
        g_soldier_office_rename_name_entry_initialized = 0;
    }
}

/* Menu callback: the hiring fee shown for the highlighted row of the hire menu,
 * 1500 for row 0, 1400 for row 1 and 0 otherwise. The fee actually charged is
 * computed when the recruit is generated (soldier_office_menu_update). */
static s32 hire_fee_get(s32 unused_row) {
    s16 mode = g_soldier_office_hire_menu_thread_data.selected_index;

    if (mode == 0) {
        return 1500;
    }
    return (mode == 1) ? 1400 : 0;
}

/* Run the soldier office's fee display script with
 * hire_fee_get installed as menu value callback 0,
 * then restore the previous callback. */
void soldier_office_fee_draw(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_menu_row_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = hire_fee_get;
    menu_run_script_with_palette_mode(g_soldier_office_fee_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Display the party unit count directly while value callback 0 is disabled. */
void soldier_office_unit_count_draw(void) {
    s32 (*previous_callback)(s32);

    previous_callback = g_menu_row_callbacks[0].value;
    g_menu_row_callbacks[0].value = 0;
    g_soldier_office_unit_count_script.unit_count.value = g_soldier_office_unit_count;
    menu_run_script_with_palette_mode(&g_soldier_office_unit_count_script, 0, 0);
    g_menu_row_callbacks[0].value = previous_callback;
}
