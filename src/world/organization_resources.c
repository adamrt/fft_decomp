/* Location lists, destination confirmation, submenu text pages, common graphics and text loading, thread results and
 * control, palettes, the main menu threads, the card menu, and the formation open requests and screen dispatch. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_sound.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_card.h"
#include "fft/world_formation.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "fft/world_text.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static u8 g_world_formation_thread_status_snapshot[16] = { 0 };

static void world_menu_build_layout_sprites(menu_entry_t* entry, SPRT sprite[2], s32 unused_2);
static void world_menu_init_quad_from_record(RECT* out, const menu_entry_t* record, SPRT* primitive);
static void world_menu_start_thread_group(s32 parameter, s32 location_id, s32 window_x, s32 window_y);
static void world_menu_thread_group_supervisor_thread(void);

/* Menu thread of the location list: list every visible location by name (row color 8
 * while its color variable is clear, 4 for the current location, which starts
 * selected), show up to 7 rows and run the scrolling list. */
void world_menu_build_location_list(void) {
    s32 i;
    s32 count;
    s32 selected;

    for (i = 0, count = 0; i < WORLD_LOCATION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) != 0) {
            g_world_menu_location_list_text_ids[count] = i + TEXT_ID_LOCATION_NAME_BASE;
            if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) == 0) {
                g_world_menu_location_list_row_colors[count] = 8;
            } else {
                g_world_menu_location_list_row_colors[count] = 0;
            }
            if (i == script_get_variable(EVENT_SCRIPT_VAR_LOCATION)) {
                g_world_menu_location_list_row_colors[count] = 4;
                selected = count;
            }
            count++;
        }
    }
    if (count >= 8) {
        g_world_menu_location_list_layout.columns.row_count = 7;
        g_world_menu_location_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_world_menu_location_list_layout.columns.row_count = count;
        g_world_menu_location_list_layout.columns.hidden_rows = 0;
    }
    if (g_world_menu_location_list_layout.columns.hidden_rows < selected) {
        g_world_menu_location_list_layout.row_offset = g_world_menu_location_list_layout.columns.hidden_rows;
    } else {
        g_world_menu_location_list_layout.row_offset = selected;
    }
    g_menu_thread_menu_data[6].selected_index = selected;
    g_menu_thread_menu_data[6].window.y = (7 - g_world_menu_location_list_layout.columns.row_count) * 8 - 0x48;
    menu_run_scrolling_ability_list_thread();
}

/* Menu action of the location list: unless the current location (row color 4) is chosen,
 * request travel there (selection result 0 with the location, the current-map variable and the
 * destination request flag) and stop the two windows after this thread. Exits the thread.
 *
 * Slot 0 is stored through a plain pointer on purpose: `pending[0]` is a
 * scalar reference there, so the scheduler may sink it below the text-id
 * load as in the target. Writing g_menu_pending_selection[0] marks the
 * store as an array element and keeps it above that load. */
void world_map_confirm_destination(void) {
    s32 selection = g_menu_thread_menu_data[6].selected_index;
    s16 value;

    if (g_world_menu_location_list_row_colors[selection] != 4) {
        s16* pending = g_menu_pending_selection;

        g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
        pending[0] = 0;
        value = g_world_menu_location_list_text_ids[selection] - TEXT_ID_LOCATION_NAME_BASE;
        pending[1] = value;
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, value);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_DESTINATION;
        thread_set_parameters(g_thread_current_id + 1, 0, 0, 1);
        thread_set_parameters(g_thread_current_id + 2, 0, 0, 1);
    } else {
        g_sound_pending_sfx_id = MAIN_SFX_INVALID;
    }
    g_menu_text_palette_offset = 0;
    thread_exit_current();
}

/* Build the text-id rows of the submenu page picked in menu entry 9 (seven rows shown, the rest
 * scroll) and run the scrolling list. */
void world_menu_build_submenu_text_id_page(void) {
    s32 selection = g_menu_thread_menu_data[9].selected_index;
    s32 count = g_world_menu_submenu_page_counts[selection];
    s32 i;
    u8* src = g_world_menu_submenu_page_text_ids[selection];

    for (i = 0; i < count; i++) {
        g_world_menu_submenu_page_row_text_ids[i] = src[i] + 0x902B;
    }
    if (count < 7) {
        g_world_menu_submenu_page_layout.columns.row_count = count;
        g_world_menu_submenu_page_layout.columns.hidden_rows = 0;
    } else {
        g_world_menu_submenu_page_layout.columns.row_count = 7;
        g_world_menu_submenu_page_layout.columns.hidden_rows = count - 7;
    }
    menu_run_scrolling_ability_list_thread();
}

/* Preset the selected row of default menu entry 4. */
void world_menu_set_slot_4_cursor_index(s32 index) {
    g_world_menu_default_entries[4].selected_index = index;
}

/* Empty and unreferenced. */
void world_noop_80106138(void) {
}

/* Read WLDFACE.BIN into a heap buffer, upload its four portrait sheets to their VRAM
 * rectangles and free the buffer once the transfers finish. */
static void world_gfx_load_wldface_to_frame_buffer(void) {
    menu_portrait_sheet_t* sheets;

    g_thread_call_target = main_file_load_checked_to_heap;
    sheets = (menu_portrait_sheet_t*)thread_call_on_main_stack(DISC_SECTOR_EVENT_WLDFACE_BIN, MENU_WLDFACE_FILE_SIZE);
    LoadImage(&g_world_wldface_vram_rects, (u32*)sheets[0].pixels);
    LoadImage(&g_world_wldface_vram_rect_1, (u32*)sheets[1].pixels);
    LoadImage(&g_world_wldface_vram_rect_2, (u32*)sheets[2].pixels);
    LoadImage(&g_world_wldface_vram_rect_3, (u32*)sheets[3].pixels);
    DrawSync(0);
    main_heap_call_free(sheets);
}

/*
 * Load the common menu images, palettes, and optional extra image pages.
 *
 * The first resource is cached across calls. Each temporary allocation stays
 * alive until its queued VRAM transfers finish.
 */
static void world_menu_load_common_graphics(s32 load_extra_pages) {
    void* resource;
    world_menu_image_pages_t* pages;
    main_gfx_frame_bin_t* frame;

    if (g_main_common_menu_graphics_loaded == 0) {
        g_thread_call_target = main_file_load_checked_to_heap;
        resource = (void*)thread_call_on_main_stack(DISC_SECTOR_EVENT_ITEM_BIN, 0x8800);
        pages = resource;
        LoadImage(&g_world_menu_item_image_rect, pages->first_page);
        LoadImage(&g_world_menu_item_palette_rect, pages->second_page);
        DrawSync(0);
        main_heap_call_free(resource);
        g_main_common_menu_graphics_loaded++;
    }
    /* LoadImage transfers pairs of 16-bit colors through 32-bit buffer elements. */
    LoadImage(&g_world_menu_common_clut_rect, (u32*)&g_world_menu_common_clut_data);
    DrawSync(0);
    g_thread_call_target = main_file_load_checked_to_heap;
    resource = (void*)thread_call_on_main_stack(DISC_SECTOR_EVENT_FRAME_BIN, sizeof(main_gfx_frame_bin_t));
    frame = resource;
    LoadImage(&g_world_menu_frame_image_rect, (u32*)frame->pixels[32]);
    LoadImage(&g_world_menu_frame_palette_rect, (u32*)frame->clut);
    LoadImage(&g_world_menu_frame_palette_tail_rect, (u32*)frame->palette_tail);
    menu_set_palette_colors(&g_menu_glyph_sheet->palette);
    DrawSync(0);
    main_heap_call_free(resource);
    if (load_extra_pages != 0) {
        g_thread_call_target = main_file_load_checked_to_heap;
        resource = (void*)thread_call_on_main_stack(DISC_SECTOR_EVENT_UNIT_BIN, 0x10000);
        pages = resource;
        LoadImage(&g_world_menu_unit_image_rect_a, pages->first_page);
        LoadImage(&g_world_menu_unit_image_rect_b, pages->second_page);
        DrawSync(0);
        main_heap_call_free(resource);
    }
}

/* Set the two substitution values of default menu entry 0: text id
 * 0xA000 + text_index and `value`. */
void world_menu_set_default_entry_0_text_values(u32 text_index, u32 value) {
    g_world_menu_default_entry_0_text_values[0] = text_index + 0xA000;
    g_world_menu_default_entry_0_text_values[1] = value;
}

/* Set the value shown in default menu entry 1's text and clear the second value. */
void world_menu_set_default_entry_1_text_values(s32 value) {
    g_world_menu_default_entry_1_text_values[0] = value;
    g_world_menu_default_entry_1_text_values[1] = 0;
}

/* Variant of world_menu_load_text_1bd8 that loads no graphics and reads the
 * 0x5800-byte OPEN.LZW text file (sector 0x1C18); all 0x20 section pointers
 * come from the file, relative to the end of its offset table. */
void world_menu_load_text_1c18(void) {
    s32 i;

    menu_init_subsystem_pointers();
    g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_OPEN_LZW, 0x5800, g_world_menu_text_file_buffer);
    menu_reset_subsystems();
    script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_menu_thread_menu_data = g_world_menu_default_entries;
    for (i = 0; i < TEXT_SECTION_COUNT; i++) {
        g_text_section_pointers[i]
            = g_world_menu_text_file_buffer[i] + (u8*)&g_world_menu_text_file_buffer[TEXT_SECTION_COUNT];
    }
    script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}

/*
 * Variant of world_menu_load_text_1bd8 for a caller that is not the main
 * thread: it leaves g_thread_current_id alone, loads the common
 * graphics in mode 1 and also uploads the WLDFACE portraits.
 */
void world_menu_load_text_1bd8_with_face(void) {
    s32 i;
    s32 value;
    u8* base;
    s32* src;
    u8** dst;

    menu_init_subsystem_pointers();
    world_menu_load_common_graphics(1);
    world_gfx_load_wldface_to_frame_buffer();
    g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xE800, g_world_menu_text_file_buffer);
    menu_reset_subsystems();
    script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_menu_thread_menu_data = g_world_menu_default_entries;
    i = 0;
    src = g_world_menu_text_file_buffer;
    base = (u8*)&src[TEXT_SECTION_COUNT];
    dst = g_text_section_pointers;
    do {
        value = *src++;
        /* Byte-pointer addition reverses the target's final addu operands. */
        *dst++ = (u8*)(value + (s32)base);
        i++;
    } while (i < TEXT_SECTION_COUNT);
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_location_names_resident;
    script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}

/*
 * Set up the WORLD menu subsystems from the main thread: common graphics,
 * the 0xE800-byte WORLD.LZW menu text file (sector 0x1BD8) and the default
 * menu entries.
 *
 * The file opens with 0x20 section offsets relative to its byte 0x80; they
 * become g_text_section_pointers, with the location names taken from
 * the resident copy. The deployment script variables are reset to one squad.
 */
void world_menu_load_text_1bd8(void) {
    s32 i;
    s32 value;
    u8* base;
    s32* src;
    u8** dst;

    g_thread_current_id = THREAD_SLOT_MAIN;
    menu_init_subsystem_pointers();
    world_menu_load_common_graphics(0);
    g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xe800, g_world_menu_text_file_buffer);
    menu_reset_subsystems();
    script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_menu_thread_menu_data = g_world_menu_default_entries;
    i = 0;
    src = g_world_menu_text_file_buffer;
    base = (u8*)&src[TEXT_SECTION_COUNT];
    dst = g_text_section_pointers;
    do {
        value = *src++;
        /* Byte-pointer addition reverses the target's final addu operands. */
        *dst++ = (u8*)(value + (s32)base);
        i++;
    } while (i < TEXT_SECTION_COUNT);
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_location_names_resident;
    script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}

/* Return from WLDCORE to the formation screen: unload the scenario music, reload the face text,
 * restore the input repeat timings from the cursor-speed option, run the formation screen, then
 * resume the event saved for the return and clear the pending scenario. */
void world_formation_restore_menu_after_core_exit(void) {
    s32 profile;

    main_sound_unload_scenario_music_and_tunes();
    ClearImage(&g_world_screen_clear_rect, 0, 0, 0);
    DrawSync(0);
    VSync(0);
    world_menu_load_text_1bd8_with_face();
    DrawSync(0);
    VSync(0);
    world_formation_reset_menu_context();
    DrawSync(0);
    VSync(0);
    profile = g_main_game_options.fields.finger_cursor_repeat_speed;
    g_main_input_repeat_initial_delay = g_world_input_timing_profiles[profile].initial_delay;
    g_main_input_repeat_period = g_world_input_timing_profiles[profile].repeat_period;
    g_main_input_secondary_repeat_period = g_world_input_timing_profiles[profile].secondary_repeat_period;
    g_main_menu_scroll_accel_delay = g_world_input_timing_profiles[profile].scroll_accel_delay;
    g_main_menu_scroll_slow_step = g_world_input_timing_profiles[profile].scroll_slow_step;
    g_main_menu_scroll_fast_step = g_world_input_timing_profiles[profile].scroll_fast_step;
    world_menu_set_brightness(0x80, 0x80, 0x80);
    world_formation_run_screen(0, 0);
    world_formation_reset_menu_context();
    ClearImage(&g_world_screen_clear_rect, 0, 0, 0);
    DrawSync(0);
    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT));
    script_set_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT, 0);
    if (script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
    }
}

/* Return the menu thread result and clear it. */
s32 world_menu_take_and_clear_thread_result(void) {
    s32 result = g_world_menu_thread_result;

    g_world_menu_thread_result = 0;
    return result;
}

/* Return 1 while a WORLD dialog waits on the player (a centered box at its confirm prompt, or a
 * choice list); WLDCORE checks it before moving on. */
s32 world_menu_get_event_state_flag(void) {
    return g_world_menu_event_state_flag;
}

/* Ask a thread to stop by setting its task id to
 * THREAD_TASK_STOP_REQUEST. */
void world_thread_request_stop(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    thread->task_id = THREAD_TASK_STOP_REQUEST;
}

/* Let a dialog thread that is waiting for more text continue: switch its task to resume and set
 * its parameter 2 to 0xFFFF. */
void world_text_resume_printing(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    if (thread->task_id == THREAD_TASK_DIALOG_AWAIT_TEXT) {
        thread->task_id = THREAD_TASK_RESUME;
        thread->function_parameter_2 = 0xffff;
    }
}

/* Returns 1 while the text thread's task id is RESUME. */
s32 world_text_is_printing(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    return thread->task_id == THREAD_TASK_RESUME;
}

/* Idle thread body: yield to the next WORLD thread, forever. */
static void world_thread_idle_yield_forever(void) {
    for (;;) {
        thread_yield();
    }
}

/* Park every idle thread slot from 5 to 8 on world_thread_idle_yield_forever. */
void world_thread_start_idle_5_to_8(void) {
    s32 thread_id;

    for (thread_id = 8; thread_id >= 5; thread_id--) {
        thread_t* thread = &g_threads[thread_id];

        if (thread->is_running == 0) {
            thread_start(thread_id, world_thread_idle_yield_forever);
        }
    }
}

/* Draw the main menu text window with its dimmed palette. */
void world_menu_enable_alternate_main_palette(void) {
    g_world_menu_use_alternate_main_palette = 1;
}

/* For each idle thread slot from 8 down to 5, suspend the slot after it. */
void world_thread_stop_after_empty_slots_5_to_8(void) {
    s32 thread_id;

    for (thread_id = 8; thread_id >= 5; thread_id--) {
        thread_t* thread = &g_threads[thread_id];

        if (thread->is_running == 0) {
            thread_suspend(thread_id + 1);
        }
    }
}

/* Draw the main menu text window with its normal palette again. */
void world_menu_disable_alternate_main_palette(void) {
    g_world_menu_use_alternate_main_palette = 0;
}

/* Resets shared context before framebuffer setup and after formation or
 * memory-card sessions. */
void world_formation_reset_menu_context(void) {
    DrawSync(0);
    menu_init_subsystem_pointers();
    menu_reset_subsystems();
    text_relocate_pointer_table(g_world_menu_text_file_buffer);
    world_text_build_roster_name_list();
    DrawSync(0);
    g_menu_thread_menu_data = g_world_menu_default_entries;
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_location_names_resident;
    g_menu_text_palette_offset = 0;
}

/* Set the scroll direction and distance that decide where the main menu's second window slides. */
void world_menu_set_main_scroll_parameters(s32 direction, s32 threshold) {
    g_world_menu_main_scroll_direction = direction;
    g_world_menu_main_scroll_threshold = threshold;
}

/* Set the color the WORLD menus tint their sprites with (0x80 each is neutral). */
void world_menu_set_brightness(s32 red, s32 green, s32 blue) {
    g_world_menu_color_red = red;
    g_world_menu_color_green = green;
    g_world_menu_color_blue = blue;
}

/* The text sprite and the two frame sprites, double-buffered by the thread below. */
typedef struct world_menu_text_window_record {
    DR_MODE text_draw_mode;
    DR_MODE frame_draw_mode;
    SPRT text_sprite;
    SPRT frame_sprites[2];
} world_menu_text_window_record_t;
typedef char world_menu_text_window_record_size_must_be_84[sizeof(world_menu_text_window_record_t) == 84 ? 1 : -1];

/* Main menu text window thread:
 * sizes the entry's window to its text, clamps it on screen, redraws the text
 * image every 32 frames and, while it runs as the menu group's second window
 * (THREAD_TASK_MENU_GROUP_SECOND_WINDOW) and the main menu scrolls past 0x28,
 * slides the window in 8-pixel steps to the left edge (positive scroll
 * direction) or the right edge (otherwise). */
void world_main_menu_text_window_thread(void) {
    RECT rect;
    world_menu_text_window_record_t records[2];
    s16 width;
    s16 height;
    menu_entry_t* entry;
    s32* text_values;
    world_menu_text_window_record_t* record;
    u8* buffer;
    s32 i;
    s32 phase;
    s32 pad;
    s16 target;
    s32 x;

    entry = (menu_entry_t*)thread_get_current_parameter_1();
    record = records;
    gfx_set_draw_mode_from_origin(&record->text_draw_mode, &entry->vram_origin);
    gfx_set_draw_mode_for_texture_page(&record->frame_draw_mode, MENU_TEXTURE_PAGE_FRAME);
    world_menu_build_layout_sprites(entry, record->frame_sprites, 0);
    mem_copy_bytes_nonempty(g_world_text_substitution_values_backup, g_text_substitution_values,
        sizeof(g_world_text_substitution_values_backup));
    text_values = entry->value;
    g_text_substitution_values[0] = text_values[0];
    text_values = entry->value;
    g_text_substitution_values[1] = text_values[1];
    text_measure(&width, &height, text_find_entry(entry->text_id));
    mem_copy_bytes_nonempty(
        g_text_substitution_values, g_world_text_substitution_values_backup, sizeof(g_text_substitution_values));
    /* 10-pixel columns plus a 0x18 border, rounded up past the next multiple of four. */
    x = width * 10;
    pad = ((x & 2) == 0) * 4;
    x += 0x18;
    width = x + (x & 3) + pad;
    height = height * 16 + 16;
    entry->inner_width = width;
    entry->window.w = width;
    entry->text_image.width = width;
    entry->inner_height = height;
    entry->window.h = height;
    entry->text_image.height = height;
    /* Keep the window on screen; menu windows use centered coordinates (x and y about 0). */
    if (entry->window.x + width > 0x80) {
        entry->window.x = 0x78 - width;
    } else if (entry->window.x < -0x80) {
        entry->window.x = -0x78;
    }
    if (entry->window.y + height > 0x78) {
        entry->window.y = 0x68 - height;
    } else if (entry->window.y < -0x78) {
        entry->window.y = -0x68;
    }
    world_menu_init_quad_from_record(&rect, entry, &record->text_sprite);
    SetSemiTrans(&record->text_sprite, 1);
    SetShadeTex(&record->text_sprite, 0);
    SetSemiTrans(&record->frame_sprites[0], 1);
    SetShadeTex(&record->frame_sprites[0], 0);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(world_menu_text_window_record_t));
    target = 0;
    for (i = 0;; i++) {
        record = &records[i & 1];
        phase = i & 0x1F;
        if (phase == 0) {
            buffer = menu_build_and_upload_window_frame_image(
                entry->inner_width, entry->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            g_menu_text_state.pen.stride = entry->inner_width;
            menu_set_text_origin(10, 8);
            g_menu_text_palette_offset = 0;
            mem_copy_bytes_nonempty(g_world_text_substitution_values_backup, g_text_substitution_values,
                sizeof(g_world_text_substitution_values_backup));
            text_values = entry->value;
            g_text_substitution_values[0] = text_values[0];
            text_values = entry->value;
            g_text_substitution_values[1] = text_values[1];
            menu_display_text_entry(entry->text_id, buffer, &g_menu_text_state.pen);
            mem_copy_bytes_nonempty(g_text_substitution_values, g_world_text_substitution_values_backup,
                sizeof(g_text_substitution_values));
            LoadImage(&rect, buffer);
        }
        thread_yield();
        if (phase == 0) {
            menu_free_memory(buffer);
        }
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        if (g_world_menu_use_alternate_main_palette != 0) {
            record->text_sprite.clut = MENU_CLUT_DIMMED;
            record->frame_sprites[0].clut = 0x7C7C;
        } else {
            record->text_sprite.clut = MENU_CLUT_NORMAL;
            record->frame_sprites[0].clut = 0x7CBC;
        }
        if (g_threads[g_thread_current_id].task_id == THREAD_TASK_MENU_GROUP_SECOND_WINDOW) {
            if (g_world_menu_main_scroll_threshold > 0x28 && target == 0) {
                if (g_world_menu_main_scroll_direction > 0) {
                    if (entry->window.x >= -0x3F) {
                        target = -0x78;
                    }
                } else if (entry->window.x < -0x3F) {
                    target = 0x78 - width;
                }
            }
            if (target != 0) {
                if (entry->window.x > target) {
                    entry->window.x -= 8;
                    if (entry->window.x <= target) {
                        entry->window.x = target;
                        target = 0;
                    }
                } else if (entry->window.x < target) {
                    entry->window.x += 8;
                    if (entry->window.x >= target) {
                        entry->window.x = target;
                        target = 0;
                    }
                }
            }
            record->text_sprite.x0 = entry->window.x;
        }
        setRGB0(&record->frame_sprites[0], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        gfx_draw_or_append_gpu_primitive(&record->frame_sprites[0]);
        gfx_draw_or_append_gpu_primitive(&record->frame_draw_mode);
        setRGB0(&record->text_sprite, g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        gfx_draw_or_append_gpu_primitive(&record->text_sprite);
        gfx_draw_or_append_gpu_primitive(&record->text_draw_mode);
    }
    thread_yield();
    thread_exit_current();
}

/* Thread body that opens the game options screen through world_menu_run_main_mode. */
static void world_menu_run_main_mode_0_thread(void) {
    world_menu_run_main_mode(OPTION_MENU_GAME_OPTIONS);
}

/* Runs the main-menu mode-0 thread in the slot below the current thread,
 * handing it this thread's first parameter, then waits for it and stops. */
void world_menu_run_main_mode_0_in_previous_thread(void) {
    s32 thread_id;

    thread_start(g_thread_current_id - 1, world_menu_run_main_mode_0_thread);
    thread_id = g_thread_current_id;
    thread_set_parameters(thread_id - 1, g_threads[thread_id].function_parameter_1, 0, 0);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_exit_current();
}

/* Menu action: request the save/load screen from the world map, hand the
 * formation input state to the event input and exit the thread. */
void world_card_open_menu(void) {
    g_world_card_menu_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the tutorial categories from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_tutorial_categories(void) {
    g_world_tutorial_categories_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the fixed number panel from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_fixed_number_panel(void) {
    g_world_fixed_number_panel_request_state = 1;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the debug variable list from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_debug_variable_list(void) {
    g_world_debug_variable_list_request_state = 1;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the formation screen from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_open(void) {
    g_world_formation_open_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the world map root level from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_world_map_root(void) {
    g_world_map_root_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Save the system flags and hide the world map's layers (dots, routes,
 * background, display objects, context value, date and location marker)
 * with the display off, for a full-screen menu. */
static void world_menu_hide_world_map(void) {
    g_world_formation_saved_system_flags = g_main_system_flags;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS | MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES
        | MAIN_SYSTEM_FLAG_DISPLAY_OFF | MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE | MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS
        | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
        | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
}

/* Restore the system flags saved before the formation screen opened. */
static void world_menu_restore_world_map(void) {
    g_main_system_flags = g_world_formation_saved_system_flags;
}

/* Debug battle setup thread: add 3 to the system flags, select the world debug battle style, clear
 * the event, map arrangement, weather and time variables, and make the number entered in the menu
 * the next map. */
void world_script_reset_event_variables_thread(void) {
    g_main_system_flags += 3;
    script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 1);
    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, 0);
    script_set_variable(EVENT_SCRIPT_VAR_WEATHER, 0);
    script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, 0);
    g_wldcore_next_map_id[0] = g_world_menu_number_entry_value;
    thread_exit_current();
}

/* Steps a pending request to open the formation screen from the world map: fade out, wait for
 * the fade, run the screen, then rebuild the world map's menu threads and window, clear the
 * scratch buffer and the screen, hold a few dark frames, and fade back in. Sibling of
 * world_card_update_menu_request. */
static void world_formation_update_open_request(void) {
    s16 state = g_world_formation_open_request_state;
    s32 i;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_formation_open_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_formation_open_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_RUN) {
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        world_formation_run_screen(0, 0);
        world_formation_reset_menu_context();
        g_world_menu_screen_open_request = 0;
        world_menu_start_thread_group(g_world_menu_thread_group_parameter, g_world_menu_location_id,
            g_world_menu_location_window_x, g_world_menu_location_window_y);
        world_menu_open_entry_window(g_world_menu_open_entry_index, -0x30, -0x38);
        g_menu_thread_menu_data = g_world_menu_default_entries;
        world_menu_hide_world_map();
        wldcore_clear_screen_and_set_map_clip_rect(0);
        for (i = 0x77FF; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
        g_world_formation_open_request_state++;
        world_menu_set_brightness(0, 0, 0);
        return;
    }
    if (state < 8) {
        g_world_formation_open_request_state = state + 1;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_IN) {
        world_menu_restore_world_map();
        wldcore_fade_start_screen(0x24, 0x10);
        g_world_formation_open_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_IN) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_formation_open_request_state = WORLD_SCREEN_REQUEST_IDLE;
        }
    }
}

/* Run the formation screen opened on the given menu (index + 1), with the menu context reset
 * before and after, then clear the screen request and restore the default menu entries. */
void world_formation_run_screen_for_menu_index(s32 menu_index) {
    world_formation_reset_menu_context();
    world_formation_run_screen(menu_index + 1, 0);
    world_formation_reset_menu_context();
    g_world_menu_screen_open_request = 0;
    g_menu_thread_menu_data = g_world_menu_default_entries;
}

/* Steps a pending request to open the tutorial categories from the world
 * map: start the screen fade-out, wait for it, then reset the menu context
 * and raise MAIN_SYSTEM_FLAG_REQUEST_TUTORIAL. */
static void world_formation_update_tutorial_categories_request(void) {
    s16 state = g_world_tutorial_categories_request_state;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_tutorial_categories_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_tutorial_categories_request_state = state + 1;
        }
        return;
    }
    world_formation_reset_menu_context();
    world_menu_set_brightness(0x80, 0x80, 0x80);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_TUTORIAL;
    g_world_tutorial_categories_request_state = WORLD_SCREEN_REQUEST_IDLE;
}

/* Serve a pending request to open the debug variable list from the world
 * map: reset the menu context and raise
 * MAIN_SYSTEM_FLAG_REQUEST_VARIABLE_LIST. */
static void world_formation_update_debug_variable_list_request(void) {
    s32 state = g_world_debug_variable_list_request_state;

    if (state != 0 && state == 1) {
        g_world_menu_screen_open_request = 1;
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        g_world_debug_variable_list_request_state = 0;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_VARIABLE_LIST;
    }
}

/* Serve a pending request to open the fixed number panel from the world
 * map: reset the menu context, reload and relocate the menu text file and
 * raise MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL. */
static void world_formation_update_fixed_number_panel_request(void) {
    s32 state = g_world_fixed_number_panel_request_state;

    if (state != 0 && state == 1) {
        g_world_menu_screen_open_request = 1;
        world_formation_reset_menu_context();
        g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xE800, g_world_menu_text_file_buffer);
        text_relocate_pointer_table(g_world_menu_text_file_buffer);
        world_menu_set_brightness(0x80, 0x80, 0x80);
        g_world_fixed_number_panel_request_state = 0;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL;
    }
}

/* Sibling of world_formation_update_tutorial_categories_request driven by g_world_card_menu_request_state. */
static void world_card_update_menu_request(void) {
    s16 state = g_world_card_menu_request_state;
    s32 result;
    s32 i;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_card_menu_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_card_menu_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_RUN) {
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        result = world_card_run_menu_screen(1);
        world_formation_reset_menu_context();
        g_world_menu_screen_open_request = 0;
        g_menu_thread_menu_data = g_world_menu_default_entries;
        world_menu_hide_world_map();
        if (result == 1) {
            g_world_card_menu_request_state = WORLD_SCREEN_REQUEST_IDLE;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_RESTART;
            return;
        }
        world_menu_start_thread_group(g_world_menu_thread_group_parameter, g_world_menu_location_id,
            g_world_menu_location_window_x, g_world_menu_location_window_y);
        world_menu_open_entry_window(g_world_menu_open_entry_index, -0x30, -0x38);
        wldcore_clear_screen_and_set_map_clip_rect(0);
        for (i = 0x77FF; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
        g_world_card_menu_request_state++;
        world_menu_set_brightness(0, 0, 0);
        return;
    }
    if (state < 8) {
        g_world_card_menu_request_state = state + 1;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_IN) {
        world_menu_restore_world_map();
        wldcore_fade_start_screen(0x24, 0x10);
        g_world_card_menu_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_IN) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_card_menu_request_state = WORLD_SCREEN_REQUEST_IDLE;
        }
    }
}

/* Sibling of world_formation_update_tutorial_categories_request driven by g_world_map_root_request_state. */
static void world_formation_update_world_map_root_request(void) {
    s16 state = g_world_map_root_request_state;

    if (state == WORLD_SCREEN_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_START_FADE_OUT) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_map_root_request_state++;
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_WAIT_FADE_OUT) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_world_map_root_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_SCREEN_REQUEST_RUN) {
        world_formation_reset_menu_context();
        g_world_map_root_request_state = WORLD_SCREEN_REQUEST_IDLE;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_BRAVE_STORY;
    }
}

/* Records the location window request and starts the menu thread-group supervisor on thread 2, unless a
 * menu screen is already opening. */
static void world_menu_start_thread_group(s32 parameter, s32 location_id, s32 window_x, s32 window_y) {
    if (g_world_menu_screen_open_request == 0) {
        if (g_world_menu_location_id != location_id) {
            g_threads[12].task_id = THREAD_TASK_NONE;
        }
        g_world_menu_location_text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_world_menu_thread_group_parameter = parameter;
        g_world_menu_location_id = location_id;
        g_world_menu_location_window_x = window_x;
        g_world_menu_location_window_y = window_y;
        g_world_menu_default_entries[2].text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_world_menu_default_entries[2].window.x = window_x;
        g_world_menu_default_entries[2].window.y = window_y;
        thread_start(WORLD_THREAD_SLOT_MENU_GROUP, world_menu_thread_group_supervisor_thread);
        thread_set_parameters(WORLD_THREAD_SLOT_MENU_GROUP, parameter, 0, 0);
    }
}

/* Menu thread-group dispatcher thread. Each request in parameter 1 selects a
 * group: running members with the expected task are signaled to continue,
 * others are signaled to finish (state 1) and awaited, then every finished
 * or idle member is restarted with its function, the matching menu entry, and
 * its task id. The thread exits once no further request is pending. */
static void world_menu_thread_group_supervisor_thread(void) {
    s32 state[16];
    world_menu_thread_group_t* group;
    s32 index;
    s32 i;
    s32 thread_id;

    for (;;) {
        index = g_threads[g_thread_current_id].function_parameter_1;
        g_threads[g_thread_current_id].function_parameter_1 = 0;
        if (index >= 4) {
            index = 0;
        }
        group = &g_world_menu_thread_groups[index];
        for (i = 15; i >= 0; i--) {
            state[i] = 0;
        }
        for (i = 0; i < 3; i++) {
            thread_id = group->thread_id[i];
            if (thread_id != -1) {
                if (g_threads[thread_id].is_running != 0) {
                    if (g_threads[thread_id].task_id != group->task_id[i]) {
                        state[thread_id] = 1;
                        thread_set_parameters(group->thread_id[i], 0, 0, 1);
                    } else {
                        thread_set_parameters(thread_id, 0, 1, 0);
                    }
                } else {
                    state[thread_id] = 2;
                }
            }
        }
        for (;;) {
            for (i = 0; i < 3; i++) {
                if (state[group->thread_id[i]] == 1 && g_threads[group->thread_id[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 3) {
                break;
            }
            thread_wait_frames(1);
        }
        for (i = 0; i < 3; i++) {
            if (state[group->thread_id[i]] != 0 && group->function[i] != (void (*)(void))-1) {
                thread_start(group->thread_id[i], group->function[i]);
                thread_set_parameters(group->thread_id[i], (s32)&g_menu_thread_menu_data[i], 0, 0);
                g_threads[group->thread_id[i]].task_id = group->task_id[i];
            }
        }
        if (g_threads[g_thread_current_id].function_parameter_1 == 0) {
            thread_exit_current();
        }
    }
}

/* Set the text image's upload rectangle and initialize its sprite. */
static void world_menu_init_quad_from_record(RECT* out, const menu_entry_t* record, SPRT* primitive) {
    out->x = record->vram_origin.x;
    out->y = record->vram_origin.y;
    out->w = record->inner_width >> 2;
    out->h = record->inner_height;
    menu_init_sprt(primitive);
    primitive->clut = MENU_CLUT_NORMAL;
    gfx_init_image_loading(primitive, &record->vram_origin, &record->window, &record->text_image);
}

/* Set up the two menu sprites for one record. The first sprite is positioned
 * from the record's x/y according to its layout mode (1 or 2), the second is
 * a fixed 16x16 sprite. Both use CLUT 0x7d7c.
 *
 * `mode` is reused to carry the y coordinate once the mode test is done; that
 * reuse is what puts the y value in the target's v1. */
static void world_menu_build_layout_sprites(menu_entry_t* entry, SPRT sprite[2], s32 unused_2) {
    u16 x;
    s32 mode;

    menu_init_sprt(sprite);
    sprite->clut = 0x7D7C;
    mode = entry->header_id;
    if (mode == 1) {
        x = entry->window.x;
        sprite->x0 = x;
        mode = (u16)entry->window.y;
        sprite->u0 = 0xA8;
        sprite->v0 = 0x88;
        sprite->w = 0x28;
        sprite->h = 8;
        mode -= 1;
        sprite->y0 = mode;
    } else {
        if (mode == 2) {
            x = entry->window.x;
            sprite->x0 = x - 6;
            mode = (u16)entry->window.y;
            sprite->u0 = 0xA8;
            sprite->v0 = 0x78;
            sprite->w = 0x28;
            sprite->h = 0x10;
            mode -= 6;
            sprite->y0 = mode;
        }
    }
    sprite++;
    menu_init_sprt(sprite);
    sprite->w = 0x10;
    sprite->h = 0x10;
    sprite->clut = 0x7D7C;
}

/* Set which buttons the formation menu accepts. */
void world_formation_set_menu_allowed_buttons(u32 mask) {
    g_world_formation_menu_allowed_buttons = mask;
}

/* The six 32-bit input masks are g_world_formation_menu_input_state,
 * but a struct global makes GCC keep a base register for the field accesses
 * (see menu_input_update_event_state), so they are separate scalars here. */
extern u32 g_world_formation_menu_current_input, g_world_formation_menu_previous_input,
    g_world_formation_menu_initial_repeat_counter, g_world_formation_menu_repeat_counter,
    g_world_formation_menu_secondary_repeat_counter;

/* Per-frame formation-menu step: loads the input timing profile selected by
 * the finger cursor repeat speed option, derives the edge/auto-repeat 32-bit input mask
 * (masked by the formation entry mask and cleared while any of the six
 * g_world_formation_open_request_state.. flags is set), runs the formation/memory-card request
 * updates, publishes the input, yields once while tracking thread status
 * changes and returns the menu result slot g_menu_pending_selection[0] (-2 while pending).
 * Formation-menu sibling of menu_input_update_event_state. */
s16 world_formation_run_menu_frame(u32* otag, u32 input) {
    s32 profile = g_main_game_options.fields.finger_cursor_repeat_speed;
    u32 previous_input;
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_delay;
    u32 counter;
    s32 thread_id;
    s32 status;

    g_main_input_repeat_initial_delay = g_world_input_timing_profiles[profile].initial_delay;
    g_main_input_repeat_period = g_world_input_timing_profiles[profile].repeat_period;
    g_main_input_secondary_repeat_period = g_world_input_timing_profiles[profile].secondary_repeat_period;
    g_main_menu_scroll_accel_delay = g_world_input_timing_profiles[profile].scroll_accel_delay;
    g_main_menu_scroll_slow_step = g_world_input_timing_profiles[profile].scroll_slow_step;
    g_main_menu_scroll_fast_step = g_world_input_timing_profiles[profile].scroll_fast_step;
    if (input == 0) {
        g_world_formation_menu_initial_repeat_counter = 0;
        g_world_formation_menu_repeat_counter = 0;
        g_world_formation_menu_secondary_repeat_counter = 0;
    }

    previous_input = g_world_formation_menu_current_input;
    g_world_formation_menu_current_input = input;
    g_world_formation_menu_new_buttons = 0;
    g_world_formation_menu_previous_input = previous_input;

    if (previous_input != input || (filtered_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_world_formation_menu_new_buttons = ~previous_input & input;
        g_world_formation_menu_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_world_formation_menu_initial_repeat_counter;
        if (counter == initial_delay) {
            g_world_formation_menu_new_buttons = filtered_input;
        }
        if (counter >= initial_delay) {
            repeat_delay = g_main_input_repeat_period / g_script_event_speed;
            counter = ++g_world_formation_menu_repeat_counter;
            g_world_formation_menu_secondary_repeat_counter++;
            if (counter >= repeat_delay) {
                g_world_formation_menu_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_world_formation_menu_new_buttons = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_world_formation_menu_new_buttons = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_delay = g_main_input_secondary_repeat_period / g_script_event_speed;
            if (g_world_formation_menu_secondary_repeat_counter >= secondary_delay) {
                g_world_formation_menu_secondary_repeat_counter = 0;
                if ((g_world_formation_menu_current_input & PSX_PAD_SQUARE) != 0) {
                    g_world_formation_menu_new_buttons
                        |= g_world_formation_menu_current_input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_world_formation_menu_new_buttons
                        |= g_world_formation_menu_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_world_formation_menu_repeat_counter = 0;
            g_world_formation_menu_secondary_repeat_counter = 0;
        }
    }

    g_world_formation_menu_new_buttons &= g_world_formation_menu_allowed_buttons;
    if (g_world_formation_open_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_tutorial_categories_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_debug_variable_list_request_state != 0) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_card_menu_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_map_root_request_state != WORLD_SCREEN_REQUEST_IDLE) {
        g_world_formation_menu_new_buttons = 0;
    }
    if (g_world_fixed_number_panel_request_state != 0) {
        g_world_formation_menu_new_buttons = 0;
    }
    g_formation_screen_active = 1;
    world_formation_update_open_request();
    world_formation_update_world_map_root_request();
    world_card_update_menu_request();
    world_formation_update_tutorial_categories_request();
    world_formation_update_debug_variable_list_request();
    world_formation_update_fixed_number_panel_request();
    g_menu_pending_selection[0] = -2;
    g_gfx_current_otag_entry = otag;
    g_sound_pending_sfx_id = -1;
    menu_draw_active_window_frames();
    script_load_event_input_state(&g_world_formation_menu_input_state);
    if (g_world_formation_menu_new_buttons != PSX_PAD_SELECT && g_world_formation_menu_new_buttons != 0) {
        g_script_input_suppression_frames = 5;
    }

    for (thread_id = 1; thread_id < THREAD_ID_COUNT; thread_id++) {
        g_world_formation_thread_status_snapshot[thread_id] = thread_is_running_asm(thread_id);
    }

    thread_yield();
    for (thread_id = 1; thread_id < THREAD_ID_COUNT; thread_id++) {
        status = g_world_formation_thread_status_snapshot[thread_id];
        if (status != thread_is_running_asm(thread_id)) {
            g_script_input_suppression_frames = 5;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }
    if (g_menu_companion_executable_request_index != 0) {
        g_menu_companion_executable_request_index = 0;
    }
    g_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_sound_pending_sfx_id != -1) {
        main_sound_play_sfx_find_channel(g_sound_pending_sfx_id);
    }
    if (g_menu_pending_selection[0] == 0) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, g_menu_pending_selection[1]);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_DESTINATION;
    }
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
    return g_menu_pending_selection[0];
}

/* Opens menu entry `index` in THREAD_SLOT_MENU, first placing its window at the default
 * origin (-0x70, -0x50; the x and y arguments are ignored) and clamping that origin so the
 * window stays on screen. Does nothing while that slot runs. */
void world_menu_open_entry_window(s32 index, s32 unused_x, s32 unused_y) {
    menu_entry_t* entry;
    s32 x;
    s32 y;
    s32 initial_x = -0x70;
    s32 initial_y = -0x50;

    if (g_threads[THREAD_SLOT_MENU].is_running == 0) {
        entry = &g_world_menu_default_entries[index];
        g_menu_thread_menu_data = g_world_menu_default_entries;
        entry->window.x = initial_x;
        entry->window.y = initial_y;
        x = entry->window.x;
        g_world_menu_open_entry_index = index;
        if (x + entry->window.w >= 0x81) {
            entry->window.x = 0x78 - entry->window.w;
        } else if (x < -0x80) {
            entry->window.x = -0x78;
        }
        y = entry->window.y;
        if (y + entry->window.h >= 0x79) {
            entry->window.y = 0x70 - entry->window.h;
        } else if (y < -0x78) {
            entry->window.y = -0x70;
        }
        thread_start(THREAD_SLOT_MENU, g_menu_thread_menu_data[index].thread_entry);
        thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[index], 0, 0);
    }
}

/* Build a 4bpp window-frame image from the shared menu graphics.
 *
 * Top and bottom bands are fixed; the middle 16 rows tile. The unused `s32`
 * return keeps $v0 live and preserves the target loop scheduling; the target
 * leaves the failed loop test's 0 there, or the partial 16-bit-element flag when `h` is
 * not positive.
 */
s32 world_menu_build_window_frame_image(s32 w, s32 h, u16* dst) {
    u16* src = (u16*)g_menu_glyph_sheet->pixels;
    s32 words;
    s32 i;
    s32 first = 0;
    u16* base = dst;
    s32 src_index;
    u16* row;
    s32 has_partial_word;

    words = w & 3;
    has_partial_word = words != 0;
    words = (w >> 2) + has_partial_word;
    for (i = 0; i < h; i++) {
        src_index = i << 6;
        if (i >= 8) {
            if (i >= h - 8) {
                src_index = (((i - h + 8) & 7) << 6) + 0x600;
            } else {
                src_index = (((i - 8) & 0xF) + 8) << 6;
            }
        }
        row = base + (i - first) * words;
        row[words - 1] = src[src_index + 9];
        row[words - 2] = src[src_index + 8];
        world_screen_menu_build_window_image_row(src, row, words, src_index);
    }
}
