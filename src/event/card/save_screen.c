/* The save screen, including slot selection, slot scans, writes, the save buffer, playtime, descriptions and the
 * scrolling slot list. */

#include "fft/bits.h"
#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/item.h"
#include "fft/memcard.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/party.h"
#include "fft/proposition.h"
#include "fft/save.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sound_novel.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "psx/kernel.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Save-image prefix and the additional bytes cleared with its working storage. */
enum { SAVE_WORK_BUFFER_BYTES = 0x1e80 };
extern u8 g_save_buffer[SAVE_WORK_BUFFER_BYTES];

static void save_build_file_header(s32 slot, s32 level, save_file_header_t* header);
static void save_build_slot_description(s32 mode, u8* destination);
static void save_init_menu_state(void);
static s32 save_init_slot_cursor(void);
static void save_render_slot_text(s32 slot);
static void save_update_slot_list(u16 input);
static void save_update_card_selection(void);
static void save_update_prompt(void);
static void save_update_slot_playtime(s32 slot);
static void save_update_scan(void);
static void save_update_scan_result(void);
static void save_update_slot_scroll(s32 selection);
static void save_update_write(s32 slot);

/* Mark no card menu row and no save slot as selected (-1 each). Nothing on the disc
 * calls it. */
void save_clear_selection(void) {
    g_save_menu_selection_index = -1;
    g_save_slot_index = -1;
}

/*
 * Load the save screen's VRAM images and start the fade-in.
 *
 * Binds the overlay's text sections, uploads the menu glyph image to
 * (0x100, 0) and the save menu palettes to (0x100, 0xfa), and derives four
 * more palette rows at y 0xfe from the CLUT rows at (0x3c0, 0x1f0) and
 * (0x3c0, 0x1f4): each row is copied with entry 4 made transparent, and its
 * entries 4..15 are copied again as a 12-color row. Sets the menu state to
 * the initial prompt and suspends thread 15.
 */
static void save_init_graphics(void) {
    RECT rect;
    palette_t image_data;

    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_text_data;
    g_text_section_pointers[TEXT_SECTION_FORMATION_MESSAGES] = g_save_text_command_help;

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);

    rect.x = 0x100;
    rect.y = 0xfa;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_save_menu_clut);

    rect.x = 0x3c0;
    rect.y = 0x1f0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, image_data);
    *(u16*)&image_data[4] = 0;

    rect.x = 0x120;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, image_data);

    rect.x = 0x100;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &image_data[4]);

    rect.x = 0x3c0;
    rect.y = 0x1f4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, image_data);
    /* Fieldwise or aggregate clearing adds instructions under this routine's -O0 profile. */
    *(u16*)&image_data[4] = 0;

    rect.x = 0x130;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, image_data);

    rect.x = 0x110;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &image_data[4]);

    gfx_start_decreasing_fade();
    g_save_menu_state = SAVE_MENU_INITIAL_PROMPT;
    thread_suspend(15);
}

/*
 * The save screen: the CARD overlay's entry for companion overlay state 5,
 * called from the event frame loops of BATTLE and WORLD.
 *
 * Builds its graphics context on its own stack frame, starts the save music
 * and the backdrop thread, and runs one frame per iteration until the closing
 * fade completes: the handler of g_save_menu_state (initial prompt, card
 * selection, scan, scan result, slot list, write), the window frames, the
 * event input update and the frame's sound effect. A LEAVE state fades the
 * music, waits for the pending card operation and starts the closing fade.
 * The event input is the held d-pad plus the newly pressed buttons; the face
 * and shoulder buttons are passed on only while a text, child-menu or
 * slot-14 thread runs. The save-file directory buffer also lives on this frame.
 */
void card_entrypoint(s32 unused) {
    TILE tile_storage[20];
    u32 otag_storage[0x80];
    save_gfx_context_t context_storage[2];
    /* The retail frame reserves 16 bytes beyond the two contexts. */
    u8 unused_context_tail[0x10];
    s32 leading_alignment;
    s32 input;
    s32 unused_local;
    s32 buttons;
    u16 menu_input;
    u16 alignment;
    s16 music_slot;
    DIRENTRY save_buffer[15];

    g_gfx_otag_count = 0x40;
    g_gfx_poly_ft4_capacity = 0x258;
    g_gfx_tile_capacity = 10;
    save_gfx_init_contexts(context_storage, otag_storage, 0, 0, 0, g_gfx_poly_ft4_buffers, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        tile_storage, 0, 0, 0, 0, 0, 0, 0, 0);
    save_init_graphics();
    music_slot = music_load_into_free_slot(MUSIC_SAVE);
    while (music_is_loading_stub() != 0) { }
    music_switch_track(music_slot, 0x7f, 0);
    memcard_consume_all_events();
    g_save_menu_work_buffer = save_buffer;
    save_init_menu_state();
    thread_start(THREAD_BORROWED_SLOT_CARD_BACKDROP, save_backdrop_thread);
    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, 0, 0);
    g_save_initial_prompt_timer = 0;

    while (gfx_update_increasing_fade() != 0) {
        save_gfx_submit_frame(0, 0);
        save_update_input();
        gfx_update_decreasing_fade();
        gfx_enqueue_tile(&g_save_gfx_fullscreen_fade_rect, &g_save_gfx_fade_overlay_color, 0, 0);

        menu_input = g_save_menu_state == SAVE_MENU_SELECT_SAVE && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0
            ? g_menu_input_primary_repeat
            : 0;
        if (g_save_menu_state >= SAVE_MENU_SELECT_SAVE && g_save_menu_state <= SAVE_MENU_WRITING) {
            save_update_slot_list(menu_input);
        }

        if (g_save_menu_state == SAVE_MENU_INITIAL_PROMPT) {
            save_update_prompt();
        } else if (g_save_menu_state == SAVE_MENU_SELECT_CARD) {
            save_update_card_selection();
        } else if (g_save_menu_state == SAVE_MENU_SCANNING) {
            save_update_scan();
        } else if (g_save_menu_state == SAVE_MENU_SCAN_RESULT) {
            save_update_scan_result();
        } else if (g_save_menu_state == SAVE_MENU_WRITING) {
            save_update_write(g_save_list_selected_slot);
        } else if (g_save_menu_state == SAVE_MENU_LEAVE || g_save_menu_state == SAVE_MENU_LEAVE_ALT) {
            music_set_target(0, 0xf0);
            while (memcard_poll_operation_result() == MEMCARD_RESULT_PENDING) { }
            gfx_start_increasing_fade();
            g_save_menu_state = SAVE_MENU_CLOSING;
        }

        save_gfx_set_render_otag(g_save_gfx_context->otag + 1);
        g_gfx_current_otag_entry = g_save_gfx_context->otag + 0x3c;
        menu_draw_active_window_frames();
        input = PadRead(0) & PSX_PAD_DPAD_MASK;
        buttons = g_menu_input_newly_pressed & 0x0fff;
        input |= buttons;
        if (thread_is_running(THREAD_SCREEN_SLOT_TEXT) == 0 && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0
            && thread_is_running(14) == 0) {
            input &= 0xff00;
        }
        menu_input_update_event_state(g_save_gfx_context->otag + 0x3c, input, g_save_gfx_drawenv_y == 0 ? 0xf0 : 0);

        if (g_sound_pending_sfx_id == SOUND_SFX_CURSOR_MOVE || g_sound_pending_sfx_id == SOUND_SFX_CANCEL) {
            g_save_sound_queued_effect_id = g_sound_pending_sfx_id;
        }
        if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = SOUND_SFX_NONE;
        }
        input = g_sound_pending_sfx_id != SOUND_SFX_NONE ? g_sound_pending_sfx_id : g_save_sound_queued_effect_id;
        if (input != SOUND_SFX_NONE) {
            sound_play_sfx(input);
        }
        g_save_sound_queued_effect_id = SOUND_SFX_NONE;
        g_sound_pending_sfx_id = SOUND_SFX_NONE;
    }

    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, -1, 0);
    music_unload_scenario_slot(music_slot);
}

/* Save-menu state SAVE_MENU_INITIAL_PROMPT: after 40 frames, runs the
 * "save the game?" menu. Choosing row 0 goes on to card selection; anything
 * else leaves the screen. */
static void save_update_prompt(void) {
    if (g_save_initial_prompt_thread_state == 0) {
        if (g_save_initial_prompt_timer < 40) {
            g_save_initial_prompt_timer++;
        } else {
            g_save_initial_prompt_thread_state = 1;
            g_save_list_cursor_index = 0;
            g_save_initial_prompt_descriptor[1].selected_index = 0;
            g_save_slot_index = -1;
        }
    } else {
        g_save_initial_prompt_thread_state
            = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_initial_prompt_descriptor);
        if (g_save_initial_prompt_thread_state == 0) {
            if (g_save_slot_index != 0) {
                g_save_menu_state = SAVE_MENU_LEAVE;
            } else {
                g_save_menu_state = SAVE_MENU_SELECT_CARD;
            }
        }
    }
}

/*
 * Save-menu state SAVE_MENU_SELECT_CARD: runs the "slot 1 / slot 2" menu.
 *
 * Canceling returns to the initial prompt, or leaves the screen once a save
 * has been completed. Choosing a card slot selects it for the card I/O,
 * forgets the last used save slot when the card slot changed, and starts the
 * scan.
 */
static void save_update_card_selection(void) {
    if (g_save_slot_selection_thread_state == 0) {
        g_save_slot_selection_thread_state = 1;
        g_save_list_cursor_index = 0;
        g_save_card_slot_selection_descriptor[1].selected_index = g_save_selected_slot;
        g_save_slot_index = -1;
    }

    g_save_slot_selection_thread_state
        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_card_slot_selection_descriptor);
    if (g_save_slot_selection_thread_state == 0) {
        if (g_save_slot_index == -1) {
            if (g_save_completed != 0) {
                g_save_menu_state = SAVE_MENU_LEAVE;
            } else {
                g_save_menu_state = SAVE_MENU_INITIAL_PROMPT;
            }
        } else {
            g_text_substitution_values[0] = g_save_slot_index + 1;
            if (g_save_selected_slot != g_save_slot_index) {
                g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            }
            g_save_selected_slot = g_save_slot_index;
            memcard_set_selected_slot(g_save_slot_index);
            g_save_menu_state = SAVE_MENU_SCANNING;
        }
    }
}

/*
 * Save-menu state SAVE_MENU_SCAN_RESULT: acts on what the card scan found.
 *
 * A card error or removal stops the notices and rescans. A pending notice
 * (set by the scan, or the "no data" notice when all 15 slots are
 * unavailable) is shown until Circle or Cross, which returns to card
 * selection. With no notice the slot list is reset to its first row and the
 * menu moves on to slot selection.
 */
static void save_update_scan_result(void) {
    s32 index;
    s32 all_empty;

    if (memcard_poll_operation_result() > MEMCARD_RESULT_COMPLETE) {
        g_save_slot_scan_prompt_descriptor = 0;
        g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
        menu_stop_thread_and_wait(14);
        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
        g_save_menu_state = SAVE_MENU_SCANNING;
    } else if (g_save_slot_scan_prompt_descriptor != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_save_slot_scan_prompt_descriptor);
        if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0 || (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
            g_save_slot_scan_prompt_descriptor = 0;
            g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_menu_state = SAVE_MENU_SELECT_CARD;
        }
    } else {
        index = 0;
        all_empty = 1;
        while (index < 15) {
            if (g_save_slot_file_states[index] != SAVE_SLOT_UNAVAILABLE) {
                all_empty = 0;
            }
            index++;
        }

        if (all_empty != 0) {
            g_save_slot_scan_prompt_descriptor = g_save_no_data_prompt_descriptor;
        }
        if (g_save_slot_scan_prompt_descriptor == 0) {
            g_save_list_scroll_base_y = 24;
            g_save_list_scroll_anim_offset = 0;
            g_save_list_top_row = 0;
            g_save_list_selected_slot = 0;
            g_save_list_cursor_index = 0;
            g_save_listed_slot_count = save_init_slot_cursor();
            g_save_menu_state = SAVE_MENU_SELECT_SAVE;
        } else {
            g_save_sound_queued_effect_id = SOUND_SFX_ERROR_MESSAGE;
        }
    }
}

/*
 * Save-menu state SAVE_MENU_WRITING: writes the game to save slot `slot`.
 *
 * The first call builds the save buffer and shows the "saving" notice; the
 * write itself happens two calls later so the notice is on screen. The
 * 0x1e00-byte file is written in one go, then reopened to write the 0x80-byte
 * slot header at offset 0x100. On success the slot is marked occupied, its
 * description, list text and play time are refreshed and it becomes the last
 * used slot. On failure the error notice stays until Circle or Cross, which
 * returns the menu to the card scan.
 */
static void save_update_write(s32 slot) {
    s32 write_result;
    s32 header_result;
    s32 unused_18;
    s32 descriptor;
    s32 unused_24;
    s32 unused_28;
    s32 unused_2c;
    s32 unused_30;
    s32 unused_34;
    s32 unused_38;
    s32 unused_3c;

    if (g_save_write_phase == 0) {
        g_save_write_phase = 1;
        g_save_write_failed = 0;
        g_save_unread_flag = 0;
        save_build_image(slot);
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_save_in_progress_thread_descriptor);
        g_sound_suppressed = 1;
        return;
    } else {
        if (g_save_write_phase < 2) {
            g_save_write_phase++;
            return;
        }
    }

    g_sound_suppressed = 0;
    if (g_save_write_failed != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_save_failure_thread_descriptor);
        if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0 || (g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_write_phase = 0;
            g_save_menu_state = SAVE_MENU_SCANNING;
        }
        return;
    }

    write_result = memcard_file_write_buffer(
        g_save_file_names[slot], g_save_buffer_pointer, 0x1e00, g_save_slot_file_states[slot]);
    menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
    header_result = 0;
    if (write_result == 0x1e00) {
        g_save_buffer_pointer->summary.slot = slot;
        descriptor = memcard_file_open_selected_with_retries(g_save_file_names[slot], 2);
        if (descriptor >= 0) {
            header_result = memcard_file_seek_with_retries(descriptor, 0x100, 0);
            if (header_result != -1) {
                header_result = memcard_file_write_with_retries(descriptor, &g_save_buffer_pointer->summary, 0x80);
            }
            if (memcard_file_close_with_retries(descriptor) == 0) {
                header_result = 0;
            }
        }
    }

    if (write_result == 0x1e00 && header_result == 0x80) {
        g_save_menu_state = SAVE_MENU_SELECT_SAVE;
        g_save_slot_file_states[slot] = SAVE_SLOT_OCCUPIED;
        save_build_slot_description(0, g_save_slot_descriptions[slot]);
        save_render_slot_text(slot);
        bcopy(&g_save_buffer_pointer->summary, &g_save_slot_metadata[slot], sizeof(save_summary_t));
        save_update_slot_playtime(slot);
        g_save_sound_queued_effect_id = SOUND_SFX_COMPLETE;
        g_save_write_phase = 0;
        g_save_completed = 1;
        g_save_last_used_slot = slot;
        return;
    }

    g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
    g_save_write_failed = 1;
    g_save_sound_queued_effect_id = SOUND_SFX_ERROR_MESSAGE;
}

/*
 * CARD.OUT save-image serializer, runtime 0x801c01ac-0x801c09b4.
 *
 * The 0x1e00-byte memory-card image is filled from the SCUS globals, then a
 * per-128-byte-block parity bit is appended through the primary bitstream
 * writer. The summary's name, job and level come from the first Ramza in the
 * roster, or from the last roster slot when there is none.
 */
void save_build_image(s32 slot) {
    s32 i;
    s32 j;
    s32 unused_18;
    s32 day;
    s32 month;
    s32 unused_24;
    s32 unused_28;
    u8* block;
    u8 parity;
    s32 unused_38[0x40];
    s32 unused_138;
    party_unit_t* unit;

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = party_get_unit(i);
        if (unit->party_id != PARTY_ID_NONE && unit->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            break;
        }
    }

    bcopy(unit->name, g_save_buffer_pointer->summary.name, sizeof(unit->name));
    g_save_buffer_pointer->summary.name_terminator = 0xfe;
    g_save_buffer_pointer->summary.job_id = unit->job_id;
    g_save_buffer_pointer->summary.level = unit->level;

    day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    g_save_buffer_pointer->summary.date.day = day;
    g_save_buffer_pointer->summary.date.month = month;
    g_save_buffer_pointer->summary.location = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    g_save_buffer_pointer->summary.slot = 0xff;
    g_save_buffer_pointer->elapsed_seconds
        = g_save_play_time_minutes * 60 + g_save_play_time_seconds + g_save_play_time_hours * 3600;
    g_save_buffer_pointer->summary.format_version = 4;

    save_build_file_header(slot, unit->level, &g_save_buffer_pointer->header);

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        bcopy(&g_party_units[i], g_save_buffer_pointer->party_records[i],
            sizeof(g_save_buffer_pointer->party_records[i]));
    }

    bcopy(g_item_quantities, g_save_buffer_pointer->item_quantities, sizeof(g_save_buffer_pointer->item_quantities));
    bcopy(g_item_poached_quantities, g_save_buffer_pointer->poached_item_quantities,
        sizeof(g_save_buffer_pointer->poached_item_quantities));
    bcopy(g_item_location_flags, g_save_buffer_pointer->item_location_flags,
        sizeof(g_save_buffer_pointer->item_location_flags));
    g_save_buffer_pointer->_unknown_1c88 = g_save_unread_byte;
    bcopy(g_script_variable_storage, g_save_buffer_pointer->script_variables, sizeof(g_script_variable_storage));
    bcopy(&g_game_options, &g_save_buffer_pointer->options, 4);
    bcopy(g_menu_sort_orders.unit_list, g_save_buffer_pointer->unit_list_sort_order, 0xc);
    bcopy(g_menu_sort_orders.formation, g_save_buffer_pointer->formation_sort_order, 8);
    bcopy(g_menu_sort_orders.weapons_and_shields, g_save_buffer_pointer->weapons_and_shields_sort_order, 7);
    bcopy(g_menu_sort_orders.headgear, g_save_buffer_pointer->headgear_sort_order, 5);
    bcopy(g_menu_sort_orders.body_armor, g_save_buffer_pointer->body_armor_sort_order, 5);
    bcopy(g_menu_sort_orders.accessories, g_save_buffer_pointer->accessories_sort_order, 7);
    bcopy(g_menu_sort_orders.other_items, g_save_buffer_pointer->other_items_sort_order, 5);
    bcopy(g_item_weapon_page_order, g_save_buffer_pointer->weapon_page_order, 0x8a);
    bcopy(g_item_helmet_page_order, g_save_buffer_pointer->helmet_page_order, 0x1d);
    bcopy(g_item_armor_page_order, g_save_buffer_pointer->armor_page_order, 0x25);
    bcopy(g_item_accessory_page_order, g_save_buffer_pointer->accessory_page_order, 0x21);
    bcopy(g_item_page_order, g_save_buffer_pointer->item_page_order, 0x15);
    bcopy(g_brave_story_treasure_acquisition_date_bits, g_save_buffer_pointer->treasure_acquisition_date_bits, 0x35);
    bcopy(g_brave_story_land_discovery_date_bits, g_save_buffer_pointer->land_discovery_date_bits, 0x12);
    bcopy(g_proposition_last_attempt_date_bits, g_save_buffer_pointer->proposition_last_attempt_date_bits, 0x6c);
    bcopy(g_proposition_states, g_save_buffer_pointer->proposition_states,
        sizeof(g_save_buffer_pointer->proposition_states));
    bcopy(g_sound_novel_saved_readings, g_save_buffer_pointer->sound_novel_saved_records, 0x118);
    bcopy(g_sound_novel_read_section_bits, g_save_buffer_pointer->sound_novel_read_section_bits,
        sizeof(g_sound_novel_read_section_bits));
    bcopy(g_sound_novel_saved_flags, g_save_buffer_pointer->sound_novel_saved_flags,
        sizeof(g_save_buffer_pointer->sound_novel_saved_flags));
    bcopy(g_brave_story_character_ages, g_save_buffer_pointer->brave_story_character_ages,
        sizeof(g_brave_story_character_ages));
    g_save_buffer_pointer->proposition_count = g_proposition_count;
    bcopy(g_proposition_active, g_save_buffer_pointer->active_propositions,
        sizeof(g_save_buffer_pointer->active_propositions));

    bits_init_primary_reader(g_save_buffer_pointer->parity_bits);
    block = (u8*)g_save_buffer_pointer;
    for (i = 0; (u32)i < SAVE_PARITY_BLOCK_COUNT; i++) {
        parity = 0;
        bits_init_secondary_reader(block);
        for (j = 0; j < SAVE_PARITY_BITS_PER_BLOCK; j++) {
            parity += bits_read_secondary(1);
        }
        block += SAVE_PARITY_BLOCK_BYTES;
        bits_write_primary(parity & 1);
    }
}

/* Reset the save list's scroll and cursor state and fill the 0x1e80-byte
 * save buffer with 0xff. */
static void save_init_menu_state(void) {
    g_save_list_scroll_base_y = 0x18;
    g_save_list_scroll_anim_offset = 0;
    g_save_list_top_row = 0;
    g_save_list_selected_slot = 0;
    g_save_list_cursor_index = 0;
    g_save_initial_prompt_timer = 0;
    g_save_buffer_pointer = (save_image_t*)g_save_buffer;
    memset(g_save_buffer_pointer, 0xff, SAVE_WORK_BUFFER_BYTES);
}

/* Store save slot `slot`'s play time as hours (capped at 99), minutes and
 * seconds from the save buffer's elapsed seconds, or zeros for a slot that
 * is not occupied. */
static void save_update_slot_playtime(s32 slot) {
    s32 component;
    s32 unused_04;
    s32 unused_08;
    s32 elapsed_seconds;
    s32 unused_14;

    if (g_save_slot_file_states[slot] != SAVE_SLOT_OCCUPIED) {
        g_save_slot_playtimes[slot].hours = 0;
        g_save_slot_playtimes[slot].minutes = 0;
        g_save_slot_playtimes[slot].seconds = 0;
    } else {
        elapsed_seconds = g_save_buffer_pointer->elapsed_seconds;

        component = elapsed_seconds / 3600;
        if (component >= 100) {
            component = 99;
        }
        g_save_slot_playtimes[slot].hours = component;

        elapsed_seconds %= 3600;
        component = elapsed_seconds / 60;
        g_save_slot_playtimes[slot].minutes = component;

        elapsed_seconds %= 60;
        g_save_slot_playtimes[slot].seconds = elapsed_seconds;
    }
}

/*
 * Save-menu state SAVE_MENU_SCANNING: classify the selected card's 15 save slots.
 *
 * When the stored slot metadata is still current (a slot is recorded as last used
 * and the card reports no change) the descriptions are rebuilt from it at once.
 * Otherwise a few setup calls clear VRAM and show the checking message, then the card
 * is opened and its directory read. A new (unformatted) card runs the format prompt;
 * an error shows the card error notice. With the card open, each later call looks up
 * one slot's file: a readable summary makes it occupied; a missing file makes it
 * available while free blocks remain, else unavailable; an unfinished save is deleted.
 * After the last slot the state moves to SAVE_MENU_SCAN_RESULT.
 */
static void save_update_scan(void) {
    s32 result;
    s32 status;
    s32 unused_18;
    s32 unused_1c;
    s32 unused_20;
    s32 unused_24;
    s32 descriptor;
    s32 unused_30;
    s32 unused_34;
    RECT rect;

    if (g_save_error_prompt_closing != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) == 0) {
            g_save_slot_scan_active = 0;
            g_save_menu_state = SAVE_MENU_SELECT_CARD;
            g_save_error_prompt_closing = 0;
        }
        return;
    }

    if (g_save_format_delay_counter != 0) {
        g_save_format_delay_counter++;
        if (g_save_format_delay_counter == 3) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_slot_scan_active = 0;
            g_save_format_delay_counter = 0;
            memcard_format_selected_slot();
            g_save_menu_state = SAVE_MENU_SELECT_CARD;
        }
        return;
    }

    if (g_save_slot_scan_active == 0) {
        menu_input_clear_state();
        /* While a slot is recorded as last used, the stored slot metadata is current: if the card
         * reports no change, rebuild the descriptions from it and skip the directory scan. */
        if (g_save_last_used_slot < 0x10) {
            result = memcard_wait_operation_result(5);
            if (result == MEMCARD_RESULT_COMPLETE) {
                g_save_slot_scan_index = 0;
                while (g_save_slot_scan_index < SAVE_SLOT_COUNT) {
                    if (g_save_slot_file_states[g_save_slot_scan_index] >= 0) {
                        bcopy(&g_save_slot_metadata[g_save_slot_scan_index], &g_save_buffer_pointer->summary,
                            sizeof(save_summary_t));
                        save_build_slot_description(g_save_slot_file_states[g_save_slot_scan_index],
                            g_save_slot_descriptions[g_save_slot_scan_index]);
                        save_render_slot_text(g_save_slot_scan_index);
                    }
                    g_save_slot_scan_index++;
                }
                g_save_menu_state = SAVE_MENU_SCAN_RESULT;
                return;
            } else {
                g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            }
        }

        /* Setup spans three calls: clear the VRAM column at x 0x180 (it holds slots 11 to 14's text),
         * start the checking message, then one idle call. */
        if (g_save_scan_setup_step < 3) {
            if (g_save_scan_setup_step == 0) {
                rect.x = 0x180;
                rect.y = 0;
                rect.w = 0x40;
                rect.h = 0xf0;
                ClearImage(&rect, 0, 0, 0);
            }
            if (g_save_scan_setup_step == 1) {
                menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_save_checking_message_descriptor);
            }
            g_save_scan_setup_step++;
            return;
        }

        g_save_open_result = memcard_open_or_create_selected_card();
        if (g_save_open_result == MEMCARD_RESULT_COMPLETE) {
            g_save_file_count = memcard_file_enumerate_saves(g_save_menu_work_buffer, 1);
            result = 0;
            /* Re-read the directory until two reads agree, at most 15 times, two vsyncs apart. */
            while (result < 15) {
                status = memcard_file_enumerate_saves(g_save_menu_work_buffer, 1);
                if (status == g_save_file_count) {
                    break;
                } else {
                    g_save_file_count = status;
                }
                VSync(2);
                result++;
            }
            g_save_free_block_count = memcard_file_count_free_blocks(g_save_menu_work_buffer, g_save_file_count);
        } else {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_sound_queued_effect_id = SOUND_SFX_ERROR_MESSAGE;
        }

        /* Every slot starts unscanned; the scan below classifies one slot per call. */
        result = 0;
        while (result < 15) {
            g_save_slot_file_states[result] = SAVE_SLOT_UNSCANNED;
            result++;
        }
        g_save_slot_index = -1;
        g_save_format_prompt_descriptor[1].selected_index = 1;
        g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
        g_save_format_prompt_active = 1;
        g_save_slot_scan_index = 0;
        g_save_slot_scan_active = 1;
        g_save_scan_setup_step = 0;
        return;
    }

    if (g_save_open_result == MEMCARD_RESULT_NEW_CARD) {
        result = memcard_poll_operation_result();
        if (result > MEMCARD_RESULT_COMPLETE) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU_CHILD);
            g_save_slot_scan_active = 0;
        } else if (g_save_format_prompt_active == 0) {
            if (g_save_slot_index == 0) {
                g_save_format_delay_counter = 1;
            } else {
                g_save_menu_state = SAVE_MENU_SELECT_CARD;
            }
            g_save_slot_scan_active = 0;
        } else {
            g_save_format_prompt_active
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_format_prompt_descriptor);
            if (g_save_format_prompt_active == 0 && g_save_slot_index == 0) {
                menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_save_formatting_message_descriptor);
            }
        }
        return;
    }

    if (g_save_open_result == MEMCARD_RESULT_ERROR || g_save_open_result == MEMCARD_RESULT_TIMEOUT) {
        result = memcard_poll_operation_result();
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_save_card_error_prompt_descriptor);
        if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0 || (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
            menu_stop_managed_thread(THREAD_SCREEN_SLOT_MENU);
            g_save_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_save_error_prompt_closing = 1;
        } else if (result == MEMCARD_RESULT_NEW_CARD || result == MEMCARD_RESULT_COMPLETE) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_slot_scan_active = 0;
        }
        return;
    }

    if (g_save_open_result == MEMCARD_RESULT_COMPLETE) {
        menu_input_clear_state();
        if (g_save_slot_scan_index < SAVE_SLOT_COUNT) {
            status = 1;
            result = 0;
            while (result < g_save_file_count) {
                status = strcmp(g_save_file_names[g_save_slot_scan_index], g_save_menu_work_buffer[result].name);
                if (status == 0) {
                    break;
                }
                result++;
            }
            /* -1: the card holds no file for this slot; 0: found. */
            status = status ? -1 : 0;

            if (status != -1) {
                memset(g_save_buffer_pointer, 0xff, SAVE_WORK_BUFFER_BYTES);
                descriptor = memcard_file_open_selected_with_retries(g_save_file_names[g_save_slot_scan_index], 1);
                if (descriptor == -2) {
                    g_save_slot_scan_active = 0;
                    menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                    return;
                }
                if (descriptor != -1) {
                    status = memcard_file_seek_with_retries(descriptor, 0x100, 0);
                    if (status != -1) {
                        status = memcard_file_read_with_retries(descriptor, &g_save_buffer_pointer->summary, 0x80);
                    }
                    /* The slot number is written last (save_update_write), so a summary still holding
                     * 0xff marks an unfinished save: delete it and count its block free again. */
                    if (status != -1 && g_save_buffer_pointer->summary.slot >= 0x10
                        && memcard_file_delete_selected_with_retries(g_save_file_names[g_save_slot_scan_index]) != 0) {
                        g_save_free_block_count++;
                        status = 0;
                    }
                    if (memcard_file_close_with_retries(descriptor) == 0) {
                        status = -1;
                    }
                    if (status == -1) {
                        g_save_slot_scan_active = 0;
                        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                        return;
                    }
                } else if (descriptor == -1) {
                    status = 0;
                }
            }

            /* A full 0x80-byte summary read means the slot holds a save. */
            if (status == 0x80) {
                g_save_slot_file_states[g_save_slot_scan_index] = SAVE_SLOT_OCCUPIED;
            }
            if (status <= 0) {
                if (g_save_free_block_count > 0) {
                    g_save_slot_file_states[g_save_slot_scan_index] = SAVE_SLOT_AVAILABLE;
                    g_save_free_block_count--;
                } else {
                    g_save_slot_file_states[g_save_slot_scan_index] = SAVE_SLOT_UNAVAILABLE;
                }
            }
            if (g_save_slot_file_states[g_save_slot_scan_index] >= 0) {
                save_build_slot_description(
                    g_save_slot_file_states[g_save_slot_scan_index], g_save_slot_descriptions[g_save_slot_scan_index]);
                bcopy(&g_save_buffer_pointer->summary, &g_save_slot_metadata[g_save_slot_scan_index],
                    sizeof(save_summary_t));
                save_update_slot_playtime(g_save_slot_scan_index);
                save_render_slot_text(g_save_slot_scan_index);
            }
            g_save_slot_scan_index++;
        } else {
            /* A last-used slot below 0x10 lets the next scan reuse these results. */
            g_save_last_used_slot = 0;
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_menu_state = SAVE_MENU_SCAN_RESULT;
            g_save_slot_scan_active = 0;
        }
    }
}

/*
 * Compose the two-line text of a save slot into `destination`.
 *
 * Mode 1 copies text entry 14 (the empty-slot line). Mode 0 builds the line
 * for the save buffer's header: name, job name at x 0x58, entry 13 (the
 * level label) at x 0xac with the two level digits, end of line; then the
 * month name, a space, the two day digits and the location name at x 0x58.
 * Digits are stored as glyphs 0..9 and each line ends with 0xfe.
 */
static void save_build_slot_description(s32 mode, u8* destination) {
    s32 index;
    u8 stack_padding[4];
    const u8* source;

    if (mode == 1) {
        source = menu_text_find_entry_by_index(g_text_data, 14, 1);
        while ((*destination++ = *source++) != 0xfe) { }
    } else if (mode == 0) {
        index = 0;
        while (g_save_buffer_pointer->summary.name[index] != 0xfe) {
            *destination++ = g_save_buffer_pointer->summary.name[index];
            index++;
        }

        *destination++ = 0xe7;
        *destination++ = 0x58;
        source = menu_text_find_entry_by_index(g_save_text_job_name, g_save_buffer_pointer->summary.job_id, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = 0xe7;
        *destination++ = 0xac;
        source = menu_text_find_entry_by_index(g_text_data, 13, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = g_save_buffer_pointer->summary.level / 10;
        *destination++ = g_save_buffer_pointer->summary.level % 10;
        *destination++ = 0xfe;
        source = menu_text_find_entry_by_index(g_text_data, g_save_buffer_pointer->summary.date.month, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = 0xfa;
        *destination++ = g_save_buffer_pointer->summary.date.day / 10;
        *destination++ = g_save_buffer_pointer->summary.date.day % 10;
        *destination++ = 0xe7;
        *destination++ = 0x58;
        source = menu_text_find_entry_by_index(g_save_text_location_name, g_save_buffer_pointer->summary.location, 1);
        while ((*destination++ = *source++) != 0xfe) { }
    }
    *destination = 0xfe;
}

/* Render save slot `slot`'s two-line description into its 0x32 x 0x20 strip
 * of VRAM: slots 0..5 at x 0x100, 6..10 at x 0x1c0 and 11..14 at x 0x180,
 * 0x20 rows apart. */
static void save_render_slot_text(s32 slot) {
    RECT rect;

    if (slot < 6) {
        rect.x = 0x100;
        rect.y = slot * 0x20 + 0x30;
        rect.w = 0x32;
        rect.h = 0x20;
    } else if (slot < 11) {
        rect.x = 0x1c0;
        rect.y = (slot - 6) * 0x20 + 0x40;
        rect.w = 0x32;
        rect.h = 0x20;
    } else {
        rect.x = 0x180;
        rect.y = (slot - 11) * 0x20 + 0x60;
        rect.w = 0x32;
        rect.h = 0x20;
    }
    text_render_id_rows_to_vram(g_save_slot_descriptions[slot], g_save_slot_text_row_ids, &rect, 0);
}

/* Put the save list cursor on the last used slot and return the number of listed
 * slots (every slot that is not SAVE_SLOT_UNAVAILABLE). The scroll step is then
 * run until it has rested for ten calls in a row, so the list opens already scrolled
 * to the cursor. */
static s32 save_init_slot_cursor(void) {
    s32 slot;
    s32 listed_count;

    g_save_menu_script_callback = 0;
    slot = 0;
    listed_count = 0;
    while (slot < 15) {
        if (slot == g_save_last_used_slot) {
            g_save_list_selected_slot = slot;
            g_save_list_cursor_index = listed_count;
        }
        if (g_save_slot_file_states[slot] != SAVE_SLOT_UNAVAILABLE) {
            listed_count++;
        }
        slot++;
    }

    /* `slot` now counts consecutive calls at rest. */
    slot = 0;
    while (slot < 10) {
        save_update_slot_scroll(g_save_list_cursor_index);
        if (g_save_list_scroll_anim_offset == 0) {
            slot++;
        } else {
            slot = 0;
        }
    }
    return listed_count;
}

/*
 * The save-slot list, run in states SAVE_MENU_SELECT_SAVE through
 * SAVE_MENU_WRITING; only the first takes `input`.
 *
 * In the selection state: rescan if the card stops answering, run the overwrite prompt
 * while it is up (YES goes on to the write), and otherwise handle CIRCLE (write to an
 * available slot, or ask before overwriting an occupied one), CROSS (back to card
 * selection) and UP/DOWN. Every call then steps the scroll and draws each listed
 * slot's window, slot number, play time and description strip, the sliding cursor
 * and the scroll arrows.
 */
static void save_update_slot_list(u16 input) {
    s32 index;
    s32 value;
    s32 remaining;
    s32 row;
    s32 scrolling;
    s16 source_v;
    u16 texture_page;
    u16 cursor_clut;
    RECT rect;

    if (g_save_menu_state == SAVE_MENU_SELECT_SAVE) {
        /* Input is ignored until every button has been released once, so a press carried over
         * from the previous state does not act here. */
        if (g_save_list_input_armed == 0) {
            g_save_overwrite_prompt_active = 0;
            if (input == 0) {
                g_save_list_input_armed = 1;
            } else {
                input = 0;
            }
        }

        /* `row` briefly holds the card status: after an error or a new card, rescan unless the card
         * still answers. */
        row = memcard_poll_operation_result();
        if (row > MEMCARD_RESULT_COMPLETE) {
            if (memcard_wait_operation_result(3) != MEMCARD_RESULT_COMPLETE) {
                menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU_CHILD);
                g_save_overwrite_prompt_active = 0;
                g_save_menu_state = SAVE_MENU_SCANNING;
                g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            }
        } else if (g_save_overwrite_prompt_active != 0) {
            g_save_overwrite_prompt_active
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_overwrite_prompt_descriptor);
            if (g_save_overwrite_prompt_active == 0 && g_save_slot_index == 0) {
                g_save_menu_state = SAVE_MENU_WRITING;
            }
        } else {
            if ((input & PSX_PAD_CIRCLE) && g_save_list_scroll_anim_offset == 0) {
                sound_play_sfx(SOUND_SFX_CONFIRM);
                /* The cursor counts listed slots only; find the slot index it points at. */
                remaining = g_save_list_cursor_index;
                index = 0;
                for (index = 0; index < SAVE_SLOT_COUNT; index++) {
                    if (g_save_slot_file_states[index] != SAVE_SLOT_UNAVAILABLE) {
                        remaining--;
                    }
                    if (remaining < 0) {
                        break;
                    }
                }
                g_save_list_selected_slot = index;
                if (g_save_slot_file_states[g_save_list_selected_slot] == SAVE_SLOT_OCCUPIED) {
                    /* Overwriting asks first; the prompt window's y depends on the cursor's visible row. */
                    index = g_save_list_cursor_index - g_save_list_top_row;
                    switch (index) {
                    case 0:
                        index = 0x5a;
                        break;
                    case 1:
                        index = 0x88;
                        break;
                    case 2:
                        index = 0x1e;
                        break;
                    case 3:
                        index = 0x3a;
                        break;
                    }
                    g_save_overwrite_prompt_descriptor[0].window.y = index;
                    g_save_overwrite_prompt_descriptor[1].selected_index = 1;
                    g_save_slot_index = -1;
                    g_save_overwrite_prompt_active = 1;
                } else {
                    g_save_menu_state = SAVE_MENU_WRITING;
                }
            }

            if ((input & PSX_PAD_CROSS) && g_save_list_scroll_anim_offset == 0) {
                g_save_sound_queued_effect_id = SOUND_SFX_CANCEL;
                g_save_menu_state = SAVE_MENU_SELECT_CARD;
            } else if (g_save_list_scroll_anim_offset == 0) {
                index = g_save_list_cursor_index;
                if (input & PSX_PAD_DOWN) {
                    g_save_list_cursor_index++;
                    if (g_save_list_cursor_index >= g_save_listed_slot_count) {
                        g_save_list_cursor_index = g_save_listed_slot_count - 1;
                    }
                } else if ((input & PSX_PAD_UP) && g_save_list_cursor_index != 0) {
                    g_save_list_cursor_index--;
                }
                if (index != g_save_list_cursor_index) {
                    g_save_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
                }
            }
        }

        /* Leaving the selection state re-arms the release check. */
        if (g_save_menu_state != SAVE_MENU_SELECT_SAVE) {
            input = 0;
            g_save_list_input_armed = 0;
        }
    }

    save_update_slot_scroll(g_save_list_cursor_index);
    g_menu_window_rect_source = g_save_slot_window_rect_source;
    scrolling = g_save_menu_state != SAVE_MENU_SELECT_SAVE;
    for (index = 0, row = 0; index < SAVE_SLOT_COUNT; index++, row++) {
        /* Unavailable slots are not listed. */
        while (g_save_slot_file_states[index] == SAVE_SLOT_UNAVAILABLE) {
            index++;
            if (index >= SAVE_SLOT_COUNT) {
                break;
            }
        }
        if (index >= SAVE_SLOT_COUNT) {
            break;
        }

        g_save_slot_row_y = g_save_list_scroll_base_y + g_save_list_scroll_anim_offset + row * 48 + 12;
        if (g_save_slot_row_y < -48 || g_save_slot_row_y >= 241) {
            continue;
        }

        gfx_set_otag_index(10);
        value = row != g_save_list_cursor_index || scrolling != 0;
        menu_set_palette_mode(value);
        menu_draw_window_frame(&g_save_slot_window_command);

        g_save_slot_quads[3].position.y = g_save_slot_quads[4].position.y = g_save_slot_quads[0].position.y
            = g_save_slot_quads[1].position.y = g_save_slot_row_y - 14;
        g_save_slot_quads[3].position.y = g_save_slot_quads[3].position.y + 1;
        g_save_slot_quads[4].position.y = g_save_slot_quads[4].position.y + 1;
        g_save_slot_quads[1].position.y++;
        if (g_save_slot_quads[0].position.y >= 0 && g_save_slot_quads[0].position.y < 241) {
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quads[0], menu_gfx_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quads[1], menu_gfx_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quads[3], menu_gfx_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quads[4], menu_gfx_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            gfx_set_otag_index(11);
            g_save_slot_number_commands[0].number.y = g_save_slot_quads[0].position.y;
            g_save_slot_number_commands[0].number.value = index + 1;
            menu_draw_decimal_number(&g_save_slot_number_commands[0].number);
        }

        g_save_slot_quads[2].position.y = g_save_slot_quads[0].position.y;
        if (g_save_slot_quads[2].position.y >= 0 && g_save_slot_quads[2].position.y < 241) {
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quads[2], menu_gfx_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            g_save_slot_number_commands[3].number.y = g_save_slot_number_commands[2].number.y
                = g_save_slot_number_commands[1].number.y = g_save_slot_quads[0].position.y;
            g_save_slot_number_commands[3].number.value = g_save_slot_playtimes[index].seconds;
            g_save_slot_number_commands[2].number.value = g_save_slot_playtimes[index].minutes;
            g_save_slot_number_commands[1].number.value = g_save_slot_playtimes[index].hours;
            if (g_save_slot_number_commands[1].number.value >= 100) {
                g_save_slot_number_commands[1].number.value = 99;
            }
            menu_draw_decimal_number(&g_save_slot_number_commands[1].number);
            menu_draw_decimal_number(&g_save_slot_number_commands[2].number);
            menu_draw_decimal_number(&g_save_slot_number_commands[3].number);
        }

        if (g_save_slot_file_states[index] == SAVE_SLOT_AVAILABLE) {
            rect.x = 0x74;
            rect.y = g_save_slot_row_y + 4;
            rect.w = 0x28;
            rect.h = 0x10;
        } else {
            rect.x = 0x1c;
            rect.y = g_save_slot_row_y - 4;
            rect.w = 0xc8;
            rect.h = 0x1f;
        }

        /* Read the slot's text strip from where save_render_slot_text put it. */
        if (index < 6) {
            texture_page = g_menu_atlas_texture_page;
            source_v = index * 0x20 + 0x30;
        } else if (index < 11) {
            texture_page = GetTPage(0, 0, 0x1c0, 0);
            source_v = (index - 6) * 0x20 + 0x40;
        } else {
            texture_page = GetTPage(0, 0, 0x180, 0);
            source_v = (index - 11) * 0x20 + 0x60;
        }
        menu_gfx_enqueue_textured_quad(&rect, 0, source_v, 0, 0, texture_page, g_menu_window_clut, 11);
    }

    gfx_set_otag_index(15);
    /* The cursor stays on one of the four visible rows. */
    value = g_save_list_cursor_index - g_save_list_top_row;
    if (value < 0) {
        value = 0;
    }
    if (value >= 4) {
        value = 3;
    }
    g_save_slot_cursor_position.y = value * 48 + 0x28;
    menu_update_and_draw_sliding_cursor(&g_save_slot_cursor_position, &g_save_slot_cursor_animation, scrolling);

    /* cursor_clut is unused: both arrows below draw with the mode-0 CLUT. */
    if (g_menu_input_lock_mode != 0) {
        cursor_clut = g_menu_cursor_mode1_foreground_clut;
    } else {
        cursor_clut = g_menu_cursor_mode0_foreground_clut;
    }

    /* Scroll arrows: up once the list has scrolled, down while rows remain below the visible four. */
    if (g_save_list_top_row != 0) {
        rect.x = 0xe3;
        rect.y = 0x20;
        rect.w = 8;
        rect.h = 0x10;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xd8, 0, 0, 0, g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 15);
    }
    if (g_save_listed_slot_count - g_save_list_top_row >= 5) {
        rect.x = 0xe3;
        rect.y = 0xba;
        rect.w = 8;
        rect.h = 0x10;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xe0, 0, 0, 0, g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 15);
    }
}

/*
 * Keeps row `selection` inside the four visible rows of the save-slot list
 * and steps the list's scroll.
 *
 * At rest, a row above or below the window moves the top row by one and
 * starts a scroll. The animation offset then holds the signed pixels covered
 * so far, growing by 4 a call; after one row height (48) it folds into
 * g_save_list_scroll_base_y and returns to 0.
 */
static void save_update_slot_scroll(s32 selection) {
    if (g_save_list_scroll_anim_offset == 0) {
        if (selection < g_save_list_top_row) {
            g_save_list_scroll_anim_offset = 4;
            g_save_list_top_row--;
        }
        if (selection >= g_save_list_top_row + 4) {
            g_save_list_scroll_anim_offset = -4;
            g_save_list_top_row++;
        }
    } else if (g_save_list_scroll_anim_offset < 0) {
        g_save_list_scroll_anim_offset -= 4;
        if (g_save_list_scroll_anim_offset < -47) {
            g_save_list_scroll_anim_offset = 0;
            g_save_list_scroll_base_y -= 48;
        }
    } else if (g_save_list_scroll_anim_offset > 0) {
        g_save_list_scroll_anim_offset += 4;
        if (g_save_list_scroll_anim_offset >= 48) {
            g_save_list_scroll_anim_offset = 0;
            g_save_list_scroll_base_y += 48;
        }
    }
}

/* Build the 256-byte PlayStation memory-card header: the title template with
 * the 1-based save slot number and the play time as full-width Shift-JIS
 * digits (0x82 then 0x4f + digit), then the icon selected by the slot. `level`
 * is not read.
 */
static void save_build_file_header(s32 slot, s32 level, save_file_header_t* header) {
    s32 value;
    s32 digit;
    s32 divisor;
    s32 pos;
    s32 unused;
    save_icon_t* records;
    save_icon_t* record;

    header->magic0 = SAVE_HEADER_MAGIC_S;
    header->magic1 = SAVE_HEADER_MAGIC_C;
    header->icon_flags = SAVE_HEADER_ICON_ONE_FRAME;
    header->block_count = SAVE_HEADER_BLOCK_COUNT_ONE;
    memset(header->reserved, 0, 0x1C);
    slot++;
    bcopy(g_save_title_template, header->title, 0x16);

    value = slot / 10;
    header->title[0x10] = 0x82;
    header->title[0x11] = value + 0x4F;
    value = slot % 10;
    header->title[0x12] = 0x82;
    header->title[0x13] = value + 0x4F;

    pos = 0x16;

    value = g_save_play_time_hours;
    if (value >= 100) {
        value = 99;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    /* Shift-JIS full-width colon. */
    header->title[pos++] = 0x81;
    header->title[pos++] = 0x46;

    value = g_save_play_time_minutes;
    if (value >= 60) {
        value = 59;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0x81;
    header->title[pos++] = 0x46;

    value = g_save_play_time_seconds;
    if (value >= 60) {
        value = 59;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0;
    header->title[pos++] = 0;

    records = g_save_icon_records;
    value = (slot - 1) % 15;
    record = records + value;
    bcopy(record->clut, header->icon_clut, 0x20);
    bcopy(record->image, header->icon_image, 0x80);
}
