/* WORLD loads the menu resources and runs the windows, input frame and screen requests used by the world map.
 * Formation, Save and the WLDCORE menus return through the same context and thread-group lifecycle. */

#include "fft/ability_list.h"
#include "fft/command.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/location.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/overlay.h"
#include "fft/portrait.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* ITEM.BIN: one 256x256 4bpp image followed by sixteen palettes. The 0x8800-byte
 * allocation rounds this 33,280-byte file up to a whole number of sectors. */
typedef struct item_graphics {
    u32 image[8192];
    clut_t palettes;
} item_graphics_t;
typedef char item_graphics_size_must_be_33280[sizeof(item_graphics_t) == 33280 ? 1 : -1];

/* UNIT.BIN: two complete upload pages containing Formation sprites and palettes. */
typedef struct unit_graphics {
    u32 pages[2][8192];
} unit_graphics_t;
typedef char unit_graphics_size_must_be_65536[sizeof(unit_graphics_t) == 65536 ? 1 : -1];

/* World-map menu requests share the fade-out states. Formation and Save
 * then run locally, wait four dark frames (states 4 to 7), and fade back in;
 * Tutorial and Brave Story hand off to WLDCORE at state 3. */
typedef enum request_state {
    WORLD_MAP_REQUEST_IDLE = 0,
    WORLD_MAP_REQUEST_START_FADE_OUT = 1,
    WORLD_MAP_REQUEST_WAIT_FADE_OUT = 2,
    WORLD_MAP_REQUEST_RUN = 3,
    WORLD_MAP_REQUEST_START_FADE_IN = 8,
    WORLD_MAP_REQUEST_WAIT_FADE_IN = 9
} request_state_e;

/* Requested text-window group for thread slots 9, 10 and 11. The four rows
 * select no windows, the first two, all three, or only the second window. */
typedef struct menu_group {
    void (*entries[3])(void); /* 0x00; -1 means do not start a replacement */
    s32 thread_ids[3];        /* 0x0c; member thread slots */
    s32 task_ids[3];          /* 0x18 */
} menu_group_t;
typedef char menu_group_size_must_be_36[sizeof(menu_group_t) == 36 ? 1 : -1];
extern u16 g_world_map_destination_list_text_ids[];
extern s16 g_world_map_destination_list_row_colors[];
extern menu_scroll_text_layout_t g_world_map_destination_list_layout;
extern menu_entry_t g_screen_default_entries[18];
extern u8 g_screen_submenu_page_counts[];
extern u8* g_screen_submenu_page_text_ids[];
extern menu_scroll_text_layout_t g_screen_submenu_page_layout;

/* The text sprite and the two frame sprites, double-buffered by the thread below. */
typedef struct text_window_packets {
    DR_MODE text_draw_mode;
    DR_MODE frame_draw_mode;
    SPRT text_sprite;
    SPRT frame_sprites[2];
} text_window_packets_t;
typedef char text_window_packets_size_must_be_84[sizeof(text_window_packets_t) == 84 ? 1 : -1];

/* location menus and screen requests */
extern u8 g_world_map_location_names[];
extern u32 g_screen_default_entry_0_text_values[2]; /* text index + 0xa000, value */

extern s32 g_screen_default_entry_1_text_values[2]; /* value, 0 */

/* menu image upload rectangles */
/* Frame-buffer rectangles for the four WLDFACE pages; four separate objects,
 * since as RECT[4] portraits_load shares one base. */
extern RECT g_screen_wldface_vram_rects;
extern RECT g_screen_wldface_vram_rect_1;
extern RECT g_screen_wldface_vram_rect_2;
extern RECT g_screen_wldface_vram_rect_3;
extern RECT g_screen_frame_image_rect;
extern RECT g_screen_frame_palette_rect;
extern RECT g_screen_frame_palette_tail_rect;
extern RECT g_screen_item_image_rect;
extern RECT g_screen_item_palette_rect;
extern RECT g_screen_unit_image_rect_a;
extern RECT g_screen_unit_image_rect_b;
extern RECT g_screen_common_clut_rect;
extern clut_t g_screen_common_clut_data;
extern s32 g_world_map_menu_scroll_direction;
extern s32 g_world_map_menu_scroll_threshold;
extern s32 g_world_map_menu_dimmed;
extern menu_input_timing_profile_t g_screen_input_timing_profiles[];
extern RECT g_screen_clear_rect;
extern menu_group_t g_world_map_menu_groups[4];
extern s32 g_world_map_menu_group_parameter;
extern s16 g_world_map_screen_request;
extern s32 g_world_map_destination_id;
extern s32 g_world_map_destination_text_id;
extern point32_t g_world_map_destination_position;
extern s16 g_world_map_brave_story_request_state; /* request_state_e */

extern s16 g_world_map_tutorial_request_state; /* request_state_e */

extern s16 g_world_map_debug_variable_list_request_state;
extern s16 g_world_map_debug_party_number_request_state;
extern s16 g_formation_open_request_state; /* request_state_e */

extern s16 g_save_menu_request_state; /* request_state_e */

extern s16 g_screen_open_entry_index;
extern s32 g_screen_number_entry_value;

/* menu input and entries */
extern u32 g_screen_allowed_buttons; /* pad mask applied to the new presses of each menu frame */

/* The six 32-bit input masks are g_screen_input_state,
 * but a struct global makes GCC keep a base register for the field accesses
 * (see menu_input_event_state_update), so they are separate scalars here. */
extern u32 g_screen_current_input, g_screen_previous_input, g_screen_initial_repeat_counter, g_screen_repeat_counter,
    g_screen_secondary_repeat_counter;

static void portraits_load(void);
static void graphics_load(s32 load_extra_pages);
static void idle_run(void);
static void options_thread_run(void);
static void map_layers_hide(void);
static void map_layers_restore(void);
static void formation_request_update(void);
static void tutorial_request_update(void);
static void variable_list_request_update(void);
static void party_number_request_update(void);
static void save_request_update(void);
static void brave_story_request_update(void);
static void window_group_start(s32 parameter, s32 location_id, s32 window_x, s32 window_y);
static void window_group_run(void);
static void entry_text_sprite_init(RECT* out, const menu_entry_t* record, SPRT* primitive);
static void entry_frame_sprites_build(menu_entry_t* entry, SPRT sprite[2], s32 unused_2);

static u8 g_menu_thread_snapshot[16] = { 0 };

static s16 g_submenu_row_text_ids[12] = { 0 };

static u32 g_menu_text_file_buffer[16512] = { 0 };

/* 32-bit substitution values saved around the text calls. */
static s32 g_saved_text_values[32] = { 0 };

static u32 g_saved_world_map_flags = 0;

/* Menu thread of the location list: list every visible location by name (row color 8
 * while its color variable is clear, 4 for the current location, which starts
 * selected), show up to 7 rows and run the scrolling list. */
void world_map_destination_list_thread(void) {
    s32 i;
    s32 count;
    s32 selected;

    for (i = 0, count = 0; i < LOCATION_COUNT; i++) {
        if (script_variable_get(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) != 0) {
            g_world_map_destination_list_text_ids[count] = i + TEXT_ID_LOCATION_NAME_BASE;
            if (script_variable_get(i + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) == 0) {
                g_world_map_destination_list_row_colors[count] = 8;
            } else {
                g_world_map_destination_list_row_colors[count] = 0;
            }
            if (i == script_variable_get(EVENT_SCRIPT_VAR_LOCATION)) {
                g_world_map_destination_list_row_colors[count] = 4;
                selected = count;
            }
            count++;
        }
    }
    if (count >= 8) {
        g_world_map_destination_list_layout.columns.row_count = 7;
        g_world_map_destination_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_world_map_destination_list_layout.columns.row_count = count;
        g_world_map_destination_list_layout.columns.hidden_rows = 0;
    }
    if (g_world_map_destination_list_layout.columns.hidden_rows < selected) {
        g_world_map_destination_list_layout.row_offset = g_world_map_destination_list_layout.columns.hidden_rows;
    } else {
        g_world_map_destination_list_layout.row_offset = selected;
    }
    g_menu_thread_menu_data[6].selected_index = selected;
    g_menu_thread_menu_data[6].window.y = (7 - g_world_map_destination_list_layout.columns.row_count) * 8 - 0x48;
    ability_list_scrolling_thread();
}

/* Menu action of the location list: unless the current location (row color 4) is chosen,
 * request travel there (selection result 0 with the location, the current-map variable and the
 * destination request flag) and stop the two windows after this thread. Exits the thread.
 *
 * Slot 0 is stored through a plain pointer on purpose: `pending[0]` is a
 * scalar reference there, so the scheduler may sink it below the text-id
 * load as in the target. Writing g_command_pending_selection[0] marks the
 * store as an array element and keeps it above that load. */
void world_map_confirm_destination(void) {
    s32 selection = g_menu_thread_menu_data[6].selected_index;
    s16 value;

    if (g_world_map_destination_list_row_colors[selection] != 4) {
        s16* pending = g_command_pending_selection;

        g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
        pending[0] = 0;
        value = g_world_map_destination_list_text_ids[selection] - TEXT_ID_LOCATION_NAME_BASE;
        pending[1] = value;
        script_variable_set(EVENT_SCRIPT_VAR_CURRENT_MAP, value);
        g_world_map_flags |= WORLD_MAP_FLAG_REQUEST_DESTINATION;
        thread_parameters_set(g_thread_current_id + 1, 0, 0, 1);
        thread_parameters_set(g_thread_current_id + 2, 0, 0, 1);
    } else {
        g_sound_pending_sfx_id = SOUND_SFX_INVALID;
    }
    g_menu_text_palette_offset = 0;
    thread_current_exit();
}

/* Build the text-id rows of the submenu page picked in menu entry 9 (seven rows shown, the rest
 * scroll) and run the scrolling list. */
void screen_submenu_page_build(void) {
    s32 selection = g_menu_thread_menu_data[9].selected_index;
    s32 count = g_screen_submenu_page_counts[selection];
    s32 i;
    u8* src = g_screen_submenu_page_text_ids[selection];

    for (i = 0; i < count; i++) {
        g_submenu_row_text_ids[i] = src[i] + 0x902B;
    }
    if (count < 7) {
        g_screen_submenu_page_layout.columns.row_count = count;
        g_screen_submenu_page_layout.columns.hidden_rows = 0;
    } else {
        g_screen_submenu_page_layout.columns.row_count = 7;
        g_screen_submenu_page_layout.columns.hidden_rows = count - 7;
    }
    ability_list_scrolling_thread();
}

/* Preset the selected row of default menu entry 4. */
void screen_entry_4_cursor_set(s32 index) {
    g_screen_default_entries[4].selected_index = index;
}

/* Empty and unreferenced. */
void screen_noop(void) {
}

/* Read WLDFACE.BIN into a heap buffer, upload its four portrait sheets to their VRAM
 * rectangles and free the buffer once the transfers finish. */
static void portraits_load(void) {
    portrait_sheet_t* sheets;

    g_thread_call_target = file_heap_load_checked;
    sheets = (portrait_sheet_t*)thread_call_on_main_stack(DISC_SECTOR_EVENT_WLDFACE_BIN, WLDFACE_FILE_SIZE);
    LoadImage(&g_screen_wldface_vram_rects, (u32*)sheets[0].pixels);
    LoadImage(&g_screen_wldface_vram_rect_1, (u32*)sheets[1].pixels);
    LoadImage(&g_screen_wldface_vram_rect_2, (u32*)sheets[2].pixels);
    LoadImage(&g_screen_wldface_vram_rect_3, (u32*)sheets[3].pixels);
    DrawSync(0);
    heap_block_release(sheets);
}

/*
 * Load the common menu images, palettes, and optional extra image pages.
 *
 * The first resource is cached across calls. Each temporary allocation stays
 * alive until its queued VRAM transfers finish.
 */
static void graphics_load(s32 load_extra_pages) {
    void* resource;
    item_graphics_t* item;
    unit_graphics_t* units;
    gfx_frame_bin_t* frame;

    if (g_menu_common_graphics_loaded == 0) {
        g_thread_call_target = file_heap_load_checked;
        resource = (void*)thread_call_on_main_stack(DISC_SECTOR_EVENT_ITEM_BIN, 0x8800);
        item = resource;
        LoadImage(&g_screen_item_image_rect, item->image);
        LoadImage(&g_screen_item_palette_rect, (u32*)item->palettes);
        DrawSync(0);
        heap_block_release(resource);
        g_menu_common_graphics_loaded++;
    }
    /* LoadImage transfers pairs of 16-bit colors through 32-bit buffer elements. */
    LoadImage(&g_screen_common_clut_rect, (u32*)&g_screen_common_clut_data);
    DrawSync(0);
    g_thread_call_target = file_heap_load_checked;
    resource = (void*)thread_call_on_main_stack(DISC_SECTOR_EVENT_FRAME_BIN, sizeof(gfx_frame_bin_t));
    frame = resource;
    LoadImage(&g_screen_frame_image_rect, (u32*)frame->pixels[32]);
    LoadImage(&g_screen_frame_palette_rect, (u32*)frame->clut);
    LoadImage(&g_screen_frame_palette_tail_rect, (u32*)frame->palette_tail);
    menu_palette_colors_set(&g_menu_glyph_sheet->palette);
    DrawSync(0);
    heap_block_release(resource);
    if (load_extra_pages != 0) {
        g_thread_call_target = file_heap_load_checked;
        resource = (void*)thread_call_on_main_stack(DISC_SECTOR_EVENT_UNIT_BIN, 0x10000);
        units = resource;
        LoadImage(&g_screen_unit_image_rect_a, units->pages[0]);
        LoadImage(&g_screen_unit_image_rect_b, units->pages[1]);
        DrawSync(0);
        heap_block_release(resource);
    }
}

/* Set the two substitution values of default menu entry 0: text id
 * 0xA000 + text_index and `value`. */
void screen_default_entry_0_text_set(u32 text_index, u32 value) {
    g_screen_default_entry_0_text_values[0] = text_index + 0xA000;
    g_screen_default_entry_0_text_values[1] = value;
}

/* Set the value shown in default menu entry 1's text and clear the second value. */
void screen_default_entry_1_text_set(s32 value) {
    g_screen_default_entry_1_text_values[0] = value;
    g_screen_default_entry_1_text_values[1] = 0;
}

/* Variant of screen_world_text_init that loads no graphics and reads the
 * 0x5800-byte OPEN.LZW text file (sector 0x1C18); all 0x20 section pointers
 * come from the file, relative to the end of its offset table. */
void screen_open_text_init(void) {
    s32 i;

    menu_subsystem_pointers_init();
    g_thread_call_target = (void (*)(void))file_buffer_load_checked;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_OPEN_LZW, 0x5800, g_menu_text_file_buffer);
    menu_subsystems_reset();
    script_variable_set(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_menu_thread_menu_data = g_screen_default_entries;
    for (i = 0; i < TEXT_SECTION_COUNT; i++) {
        g_text_section_pointers[i] = g_menu_text_file_buffer[i] + (u8*)&g_menu_text_file_buffer[TEXT_SECTION_COUNT];
    }
    script_variable_set(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}

/*
 * Variant of screen_world_text_init for a caller that is not the main
 * thread: it leaves g_thread_current_id alone, loads the common
 * graphics in mode 1 and also uploads the WLDFACE portraits.
 */
void screen_world_resources_init(void) {
    s32 i;
    s32 value;
    u8* base;
    u32* src;
    u8** dst;

    menu_subsystem_pointers_init();
    graphics_load(1);
    portraits_load();
    g_thread_call_target = (void (*)(void))file_buffer_load_checked;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xE800, g_menu_text_file_buffer);
    menu_subsystems_reset();
    script_variable_set(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_menu_thread_menu_data = g_screen_default_entries;
    i = 0;
    src = g_menu_text_file_buffer;
    base = (u8*)&src[TEXT_SECTION_COUNT];
    dst = g_text_section_pointers;
    do {
        value = *src++;
        /* Byte-pointer addition reverses the target's final addu operands. */
        *dst++ = (u8*)(value + (s32)base);
        i++;
    } while (i < TEXT_SECTION_COUNT);
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_map_location_names;
    script_variable_set(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
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
void screen_world_text_init(void) {
    s32 i;
    s32 value;
    u8* base;
    u32* src;
    u8** dst;

    g_thread_current_id = THREAD_SLOT_MAIN;
    menu_subsystem_pointers_init();
    graphics_load(0);
    g_thread_call_target = (void (*)(void))file_buffer_load_checked;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xe800, g_menu_text_file_buffer);
    menu_subsystems_reset();
    script_variable_set(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_menu_thread_menu_data = g_screen_default_entries;
    i = 0;
    src = g_menu_text_file_buffer;
    base = (u8*)&src[TEXT_SECTION_COUNT];
    dst = g_text_section_pointers;
    do {
        value = *src++;
        /* Byte-pointer addition reverses the target's final addu operands. */
        *dst++ = (u8*)(value + (s32)base);
        i++;
    } while (i < TEXT_SECTION_COUNT);
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_map_location_names;
    script_variable_set(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}

/* Return from WLDCORE to the formation screen: unload the scenario music, reload the face text,
 * restore the input repeat timings from the cursor-speed option, run the formation screen, then
 * resume the event saved for the return and clear the pending scenario. */
void formation_world_map_return_run(void) {
    s32 profile;

    music_all_unload();
    ClearImage(&g_screen_clear_rect, 0, 0, 0);
    DrawSync(0);
    VSync(0);
    screen_world_resources_init();
    DrawSync(0);
    VSync(0);
    screen_context_reset();
    DrawSync(0);
    VSync(0);
    profile = g_game_options.fields.finger_cursor_repeat_speed;
    g_input_repeat_initial_delay = g_screen_input_timing_profiles[profile].initial_delay;
    g_input_repeat_period = g_screen_input_timing_profiles[profile].repeat_period;
    g_input_secondary_repeat_period = g_screen_input_timing_profiles[profile].secondary_repeat_period;
    g_menu_scroll_accel_delay = g_screen_input_timing_profiles[profile].scroll_accel_delay;
    g_menu_scroll_slow_step = g_screen_input_timing_profiles[profile].scroll_slow_step;
    g_menu_scroll_fast_step = g_screen_input_timing_profiles[profile].scroll_fast_step;
    screen_brightness_set(0x80, 0x80, 0x80);
    formation_screen_run(0, 0);
    screen_context_reset();
    ClearImage(&g_screen_clear_rect, 0, 0, 0);
    DrawSync(0);
    script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, script_variable_get(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT));
    script_variable_set(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT, 0);
    if (script_variable_get(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        script_variable_set(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
    }
}

/* Return the menu thread result and clear it. */
s32 screen_take_thread_result(void) {
    s32 result = g_screen_thread_result;

    g_screen_thread_result = 0;
    return result;
}

/* Return 1 while a WORLD dialog waits on the player (a centered box at its confirm prompt, or a
 * choice list); WLDCORE checks it before moving on. */
s32 screen_is_dialog_waiting(void) {
    return g_screen_dialog_waiting;
}

/* Ask a thread to stop by setting its task id to
 * THREAD_TASK_STOP_REQUEST. */
void screen_thread_request_stop(s32 thread_id) {
    g_threads[thread_id].task_id = THREAD_TASK_STOP_REQUEST;
}

/* Let a dialog thread that is waiting for more text continue: switch its task to resume and set
 * its parameter 2 to 0xFFFF. */
void screen_text_printing_resume(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    if (thread->task_id == THREAD_TASK_DIALOG_AWAIT_TEXT) {
        thread->task_id = THREAD_TASK_RESUME;
        thread->function_parameter_2 = 0xffff;
    }
}

/* Returns 1 while the text thread's task id is RESUME. */
s32 screen_text_printing_check(s32 thread_id) {
    return g_threads[thread_id].task_id == THREAD_TASK_RESUME;
}

/* Idle thread body: yield to the next WORLD thread, forever. */
static void idle_run(void) {
    for (;;) {
        thread_yield();
    }
}

/* Park every idle thread slot from 5 to 8 on idle_run. */
void screen_thread_park_slots_5_to_8(void) {
    s32 thread_id;

    for (thread_id = 8; thread_id >= 5; thread_id--) {
        if (g_threads[thread_id].is_running == 0) {
            thread_start(thread_id, idle_run);
        }
    }
}

/* Draw the main menu text window with its dimmed palette. */
void world_map_menu_dim(void) {
    g_world_map_menu_dimmed = 1;
}

/* For each idle thread slot from 8 down to 5, suspend the slot after it. */
void screen_thread_slots_5_to_8_stop(void) {
    s32 thread_id;

    for (thread_id = 8; thread_id >= 5; thread_id--) {
        if (g_threads[thread_id].is_running == 0) {
            thread_suspend(thread_id + 1);
        }
    }
}

/* Draw the main menu text window with its normal palette again. */
void world_map_menu_undim(void) {
    g_world_map_menu_dimmed = 0;
}

/* Resets shared context before framebuffer setup and after formation or
 * memory-card sessions. */
void screen_context_reset(void) {
    DrawSync(0);
    menu_subsystem_pointers_init();
    menu_subsystems_reset();
    text_pointer_table_relocate(g_menu_text_file_buffer);
    formation_roster_names_build();
    DrawSync(0);
    g_menu_thread_menu_data = g_screen_default_entries;
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_map_location_names;
    g_menu_text_palette_offset = 0;
}

/* Set the scroll direction and distance that decide where the main menu's second window slides. */
void world_map_menu_scroll_set(s32 direction, s32 threshold) {
    g_world_map_menu_scroll_direction = direction;
    g_world_map_menu_scroll_threshold = threshold;
}

/* Set the color the WORLD menus tint their sprites with (0x80 each is neutral). */
void screen_brightness_set(s32 red, s32 green, s32 blue) {
    g_screen_color_red = red;
    g_screen_color_green = green;
    g_screen_color_blue = blue;
}

/* Main menu text window thread:
 * sizes the entry's window to its text, clamps it on screen, redraws the text
 * image every 32 frames and, while it runs as the menu group's second window
 * (THREAD_TASK_MENU_GROUP_SECOND_WINDOW) and the main menu scrolls past 0x28,
 * slides the window in 8-pixel steps to the left edge (positive scroll
 * direction) or the right edge (otherwise). */
void world_map_menu_text_window_thread(void) {
    RECT rect;
    text_window_packets_t records[2];
    s16 width;
    s16 height;
    menu_entry_t* entry;
    s32* text_values;
    text_window_packets_t* record;
    u8* buffer;
    s32 i;
    s32 phase;
    s32 pad;
    s16 target;
    s32 x;

    entry = (menu_entry_t*)thread_current_parameter_1_get();
    record = records;
    gfx_set_draw_mode_from_origin(&record->text_draw_mode, &entry->vram_origin);
    gfx_set_draw_mode_for_texture_page(&record->frame_draw_mode, MENU_TEXTURE_PAGE_FRAME);
    entry_frame_sprites_build(entry, record->frame_sprites, 0);
    mem_copy_bytes_nonempty(g_saved_text_values, g_text_substitution_values, sizeof(g_saved_text_values));
    text_values = entry->value;
    g_text_substitution_values[0] = text_values[0];
    text_values = entry->value;
    g_text_substitution_values[1] = text_values[1];
    text_measure(&width, &height, text_entry_find(entry->text_id));
    mem_copy_bytes_nonempty(g_text_substitution_values, g_saved_text_values, sizeof(g_text_substitution_values));
    /* 10-pixel columns plus a 0x18 border, rounded up past the next multiple of four. */
    x = width * 10;
    pad = ((x & 2) == 0) * 4;
    x += 0x18;
    width = x + (x & 3) + pad;
    height = height * 16 + 16;
    entry->inner_width = width;
    entry->window.w = width;
    entry->text_image.source.w = width;
    entry->inner_height = height;
    entry->window.h = height;
    entry->text_image.source.h = height;
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
    entry_text_sprite_init(&rect, entry, &record->text_sprite);
    SetSemiTrans(&record->text_sprite, 1);
    SetShadeTex(&record->text_sprite, 0);
    SetSemiTrans(&record->frame_sprites[0], 1);
    SetShadeTex(&record->frame_sprites[0], 0);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(text_window_packets_t));
    target = 0;
    for (i = 0;; i++) {
        record = &records[i & 1];
        phase = i & 0x1F;
        if (phase == 0) {
            buffer = menu_build_and_upload_window_frame_image(
                entry->inner_width, entry->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            g_menu_window_text_pen.stride = entry->inner_width;
            menu_set_text_origin(10, 8);
            g_menu_text_palette_offset = 0;
            mem_copy_bytes_nonempty(g_saved_text_values, g_text_substitution_values, sizeof(g_saved_text_values));
            text_values = entry->value;
            g_text_substitution_values[0] = text_values[0];
            text_values = entry->value;
            g_text_substitution_values[1] = text_values[1];
            menu_display_text_entry(entry->text_id, buffer, &g_menu_window_text_pen);
            mem_copy_bytes_nonempty(
                g_text_substitution_values, g_saved_text_values, sizeof(g_text_substitution_values));
            LoadImage(&rect, buffer);
        }
        thread_yield();
        if (phase == 0) {
            menu_free_memory(buffer);
        }
        if (thread_current_parameter_3_get() != 0) {
            break;
        }
        if (g_world_map_menu_dimmed != 0) {
            record->text_sprite.clut = MENU_CLUT_DIMMED;
            record->frame_sprites[0].clut = 0x7C7C;
        } else {
            record->text_sprite.clut = MENU_CLUT_NORMAL;
            record->frame_sprites[0].clut = 0x7CBC;
        }
        if (g_threads[g_thread_current_id].task_id == THREAD_TASK_MENU_GROUP_SECOND_WINDOW) {
            if (g_world_map_menu_scroll_threshold > 0x28 && target == 0) {
                if (g_world_map_menu_scroll_direction > 0) {
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
        setRGB0(&record->frame_sprites[0], g_screen_color_red, g_screen_color_green, g_screen_color_blue);
        gfx_draw_or_append_gpu_primitive(&record->frame_sprites[0]);
        gfx_draw_or_append_gpu_primitive(&record->frame_draw_mode);
        setRGB0(&record->text_sprite, g_screen_color_red, g_screen_color_green, g_screen_color_blue);
        gfx_draw_or_append_gpu_primitive(&record->text_sprite);
        gfx_draw_or_append_gpu_primitive(&record->text_draw_mode);
    }
    thread_yield();
    thread_current_exit();
}

/* Thread body that opens the game options screen through option_entrypoint. */
static void options_thread_run(void) {
    option_entrypoint(MENU_OPTION_TYPE_GAME_OPTIONS);
}

/* Runs the game options thread in the slot below the current thread,
 * handing it this thread's first parameter, then waits for it and stops. */
void option_game_options_in_previous_thread_open(void) {
    s32 thread_id;

    thread_start(g_thread_current_id - 1, options_thread_run);
    thread_id = g_thread_current_id;
    thread_parameters_set(thread_id - 1, g_threads[thread_id].function_parameter_1, 0, 0);
    thread_inactive_wait(g_thread_current_id - 1);
    thread_current_exit();
}

/* Menu action: request the save/load screen from the world map, hand the
 * formation input state to the event input and exit the thread. */
void world_map_save_request(void) {
    g_save_menu_request_state = WORLD_MAP_REQUEST_START_FADE_OUT;
    g_screen_new_buttons = 0;
    script_load_event_input_state(&g_screen_input_state);
    thread_current_exit();
}

/* Menu action: request the tutorial categories from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_map_tutorial_request(void) {
    g_world_map_tutorial_request_state = WORLD_MAP_REQUEST_START_FADE_OUT;
    g_screen_new_buttons = 0;
    script_load_event_input_state(&g_screen_input_state);
    thread_current_exit();
}

/* Request the debug Party number panel, which replaces the roster from
 * an ENTD encounter, then hand input back to the event state and exit. */
void world_map_debug_party_request(void) {
    g_world_map_debug_party_number_request_state = 1;
    g_screen_new_buttons = 0;
    script_load_event_input_state(&g_screen_input_state);
    thread_current_exit();
}

/* Menu action: request the debug variable list from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_map_debug_variables_request(void) {
    g_world_map_debug_variable_list_request_state = 1;
    g_screen_new_buttons = 0;
    script_load_event_input_state(&g_screen_input_state);
    thread_current_exit();
}

/* Menu action: request the formation screen from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_map_formation_request(void) {
    g_formation_open_request_state = WORLD_MAP_REQUEST_START_FADE_OUT;
    g_screen_new_buttons = 0;
    script_load_event_input_state(&g_screen_input_state);
    thread_current_exit();
}

/* Menu action: request the Brave Story from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_map_brave_story_request(void) {
    g_world_map_brave_story_request_state = WORLD_MAP_REQUEST_START_FADE_OUT;
    g_screen_new_buttons = 0;
    script_load_event_input_state(&g_screen_input_state);
    thread_current_exit();
}

/* Save the system flags and hide the world map's layers (dots, routes,
 * background, display objects, context value, date and location label)
 * with the display off, for a full-screen menu. */
static void map_layers_hide(void) {
    g_saved_world_map_flags = g_world_map_flags;
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_MAP_DOTS | WORLD_MAP_FLAG_HIDE_MAP_ROUTES | WORLD_MAP_FLAG_DISPLAY_OFF
        | WORLD_MAP_FLAG_NO_BACKGROUND_IMAGE | WORLD_MAP_FLAG_HIDE_DISPLAY_OBJECTS | WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE
        | WORLD_MAP_FLAG_HIDE_CALENDAR_DATE | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
}

/* Restore the world-map flags saved around Formation or Save. */
static void map_layers_restore(void) {
    g_world_map_flags = g_saved_world_map_flags;
}

/* Debug battle setup thread: add 3 to the system flags, select the world debug battle style, clear
 * the event, map arrangement, weather and time variables, and make the number entered in the menu
 * the next map. */
void debug_battle_setup_thread(void) {
    g_world_map_flags += 3;
    script_variable_set(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 1);
    script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
    script_variable_set(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, 0);
    script_variable_set(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, 0);
    script_variable_set(EVENT_SCRIPT_VAR_WEATHER, 0);
    script_variable_set(EVENT_SCRIPT_VAR_TIME_OF_DAY, 0);
    g_travel_next_map_id = g_screen_number_entry_value;
    thread_current_exit();
}

/* Steps a pending request to open the formation screen from the world map: fade out, wait for
 * the fade, run the screen, then rebuild the world map's menu threads and window, clear the
 * scratch buffer and the screen, hold a few dark frames, and fade back in. Sibling of
 * save_request_update. */
static void formation_request_update(void) {
    s16 state = g_formation_open_request_state;
    s32 i;

    if (state == WORLD_MAP_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_MAP_REQUEST_START_FADE_OUT) {
        g_world_map_screen_request = 1;
        world_map_fade_start(0x12, 0x10);
        g_formation_open_request_state++;
        return;
    }
    if (state == WORLD_MAP_REQUEST_WAIT_FADE_OUT) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            g_formation_open_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_MAP_REQUEST_RUN) {
        screen_context_reset();
        screen_brightness_set(0x80, 0x80, 0x80);
        formation_screen_run(0, 0);
        screen_context_reset();
        g_world_map_screen_request = 0;
        window_group_start(g_world_map_menu_group_parameter, g_world_map_destination_id,
            g_world_map_destination_position.x, g_world_map_destination_position.y);
        screen_entry_window_open(g_screen_open_entry_index, -0x30, -0x38);
        g_menu_thread_menu_data = g_screen_default_entries;
        map_layers_hide();
        world_map_clear_screen_and_set_clip_rect(0);
        for (i = 0x77FF; i >= 0; i--) {
            g_world_map_scratch_buffer[i] = 0;
        }
        g_formation_open_request_state++;
        screen_brightness_set(0, 0, 0);
        return;
    }
    if (state < 8) {
        g_formation_open_request_state = state + 1;
        return;
    }
    if (state == WORLD_MAP_REQUEST_START_FADE_IN) {
        map_layers_restore();
        world_map_fade_start(0x24, 0x10);
        g_formation_open_request_state++;
        return;
    }
    if (state == WORLD_MAP_REQUEST_WAIT_FADE_IN) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            g_formation_open_request_state = WORLD_MAP_REQUEST_IDLE;
        }
    }
}

/* Run formation tutorial index + 1, resetting the menu context before and after,
 * then restore the default menu entries and clear the screen request. */
void formation_tutorial_run(s32 tutorial_index) {
    screen_context_reset();
    formation_screen_run(tutorial_index + 1, 0);
    screen_context_reset();
    g_world_map_screen_request = 0;
    g_menu_thread_menu_data = g_screen_default_entries;
}

/* Steps a pending request to open the tutorial categories from the world
 * map: start the screen fade-out, wait for it, then reset the menu context
 * and raise WORLD_MAP_FLAG_REQUEST_TUTORIAL. */
static void tutorial_request_update(void) {
    s16 state = g_world_map_tutorial_request_state;

    if (state == WORLD_MAP_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_MAP_REQUEST_START_FADE_OUT) {
        g_world_map_screen_request = 1;
        world_map_fade_start(0x12, 0x10);
        g_world_map_tutorial_request_state++;
        return;
    }
    if (state == WORLD_MAP_REQUEST_WAIT_FADE_OUT) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            g_world_map_tutorial_request_state = state + 1;
        }
        return;
    }
    screen_context_reset();
    screen_brightness_set(0x80, 0x80, 0x80);
    g_world_map_flags |= WORLD_MAP_FLAG_REQUEST_TUTORIAL;
    g_world_map_tutorial_request_state = WORLD_MAP_REQUEST_IDLE;
}

/* Serve a pending request to open the debug variable list from the world
 * map: reset the menu context and raise
 * WORLD_MAP_FLAG_REQUEST_VARIABLE_LIST. */
static void variable_list_request_update(void) {
    s32 state = g_world_map_debug_variable_list_request_state;

    if (state != 0 && state == 1) {
        g_world_map_screen_request = 1;
        screen_context_reset();
        screen_brightness_set(0x80, 0x80, 0x80);
        g_world_map_debug_variable_list_request_state = 0;
        g_world_map_flags |= WORLD_MAP_FLAG_REQUEST_VARIABLE_LIST;
    }
}

/* Reload the WORLD menu text and hand the debug Party number request to
 * WLDCORE, which opens the encounter-number editor. */
static void party_number_request_update(void) {
    s32 state = g_world_map_debug_party_number_request_state;

    if (state != 0 && state == 1) {
        g_world_map_screen_request = 1;
        screen_context_reset();
        g_thread_call_target = (void (*)(void))file_buffer_load_checked;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xE800, g_menu_text_file_buffer);
        text_pointer_table_relocate(g_menu_text_file_buffer);
        screen_brightness_set(0x80, 0x80, 0x80);
        g_world_map_debug_party_number_request_state = 0;
        g_world_map_flags |= WORLD_MAP_FLAG_REQUEST_DEBUG_PARTY_NUMBER;
    }
}

/* Run Save and restore the world map through the same dark-frame and fade-in
 * sequence as Formation, unless Save requests a restart. */
static void save_request_update(void) {
    s16 state = g_save_menu_request_state;
    s32 result;
    s32 i;

    if (state == WORLD_MAP_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_MAP_REQUEST_START_FADE_OUT) {
        g_world_map_screen_request = 1;
        world_map_fade_start(0x12, 0x10);
        g_save_menu_request_state++;
        return;
    }
    if (state == WORLD_MAP_REQUEST_WAIT_FADE_OUT) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            g_save_menu_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_MAP_REQUEST_RUN) {
        screen_context_reset();
        screen_brightness_set(0x80, 0x80, 0x80);
        result = save_screen_run(1);
        screen_context_reset();
        g_world_map_screen_request = 0;
        g_menu_thread_menu_data = g_screen_default_entries;
        map_layers_hide();
        if (result == 1) {
            g_save_menu_request_state = WORLD_MAP_REQUEST_IDLE;
            g_world_map_flags |= WORLD_MAP_FLAG_RESTART;
            return;
        }
        window_group_start(g_world_map_menu_group_parameter, g_world_map_destination_id,
            g_world_map_destination_position.x, g_world_map_destination_position.y);
        screen_entry_window_open(g_screen_open_entry_index, -0x30, -0x38);
        world_map_clear_screen_and_set_clip_rect(0);
        for (i = 0x77FF; i >= 0; i--) {
            g_world_map_scratch_buffer[i] = 0;
        }
        g_save_menu_request_state++;
        screen_brightness_set(0, 0, 0);
        return;
    }
    if (state < 8) {
        g_save_menu_request_state = state + 1;
        return;
    }
    if (state == WORLD_MAP_REQUEST_START_FADE_IN) {
        map_layers_restore();
        world_map_fade_start(0x24, 0x10);
        g_save_menu_request_state++;
        return;
    }
    if (state == WORLD_MAP_REQUEST_WAIT_FADE_IN) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            g_save_menu_request_state = WORLD_MAP_REQUEST_IDLE;
        }
    }
}

/* Sibling of tutorial_request_update driven by g_world_map_brave_story_request_state. */
static void brave_story_request_update(void) {
    s16 state = g_world_map_brave_story_request_state;

    if (state == WORLD_MAP_REQUEST_IDLE) {
        return;
    }
    if (state == WORLD_MAP_REQUEST_START_FADE_OUT) {
        g_world_map_screen_request = 1;
        world_map_fade_start(0x12, 0x10);
        g_world_map_brave_story_request_state++;
        return;
    }
    if (state == WORLD_MAP_REQUEST_WAIT_FADE_OUT) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            g_world_map_brave_story_request_state = state + 1;
        }
        return;
    }
    if (state == WORLD_MAP_REQUEST_RUN) {
        screen_context_reset();
        g_world_map_brave_story_request_state = WORLD_MAP_REQUEST_IDLE;
        g_world_map_flags |= WORLD_MAP_FLAG_REQUEST_BRAVE_STORY;
    }
}

/* Records the location window request and starts the menu thread-group supervisor on thread 2, unless a
 * menu screen is already opening. */
static void window_group_start(s32 parameter, s32 location_id, s32 window_x, s32 window_y) {
    if (g_world_map_screen_request == 0) {
        if (g_world_map_destination_id != location_id) {
            g_threads[12].task_id = THREAD_TASK_NONE;
        }
        g_world_map_destination_text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_world_map_menu_group_parameter = parameter;
        g_world_map_destination_id = location_id;
        g_world_map_destination_position.x = window_x;
        g_world_map_destination_position.y = window_y;
        g_screen_default_entries[2].text_id = location_id + TEXT_ID_LOCATION_NAME_BASE;
        g_screen_default_entries[2].window.x = window_x;
        g_screen_default_entries[2].window.y = window_y;
        thread_start(SCREEN_THREAD_SLOT_MENU_GROUP, window_group_run);
        thread_parameters_set(SCREEN_THREAD_SLOT_MENU_GROUP, parameter, 0, 0);
    }
}

/* Menu thread-group dispatcher thread. Each request in parameter 1 selects a
 * group: running members with the expected task are signaled to continue,
 * others are signaled to finish (state 1) and awaited. Finished or idle members
 * with callbacks are started with the matching menu entry and task id; a -1
 * callback starts no replacement. The thread exits once no request is pending. */
static void window_group_run(void) {
    s32 state[16];
    menu_group_t* group;
    s32 index;
    s32 i;
    s32 thread_id;

    for (;;) {
        index = g_threads[g_thread_current_id].function_parameter_1;
        g_threads[g_thread_current_id].function_parameter_1 = 0;
        if (index >= 4) {
            index = 0;
        }
        group = &g_world_map_menu_groups[index];
        for (i = 15; i >= 0; i--) {
            state[i] = 0;
        }
        for (i = 0; i < 3; i++) {
            thread_id = group->thread_ids[i];
            if (thread_id != -1) {
                if (g_threads[thread_id].is_running != 0) {
                    if (g_threads[thread_id].task_id != group->task_ids[i]) {
                        state[thread_id] = 1;
                        thread_parameters_set(group->thread_ids[i], 0, 0, 1);
                    } else {
                        thread_parameters_set(thread_id, 0, 1, 0);
                    }
                } else {
                    state[thread_id] = 2;
                }
            }
        }
        for (;;) {
            for (i = 0; i < 3; i++) {
                if (state[group->thread_ids[i]] == 1 && g_threads[group->thread_ids[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 3) {
                break;
            }
            thread_frames_wait(1);
        }
        for (i = 0; i < 3; i++) {
            if (state[group->thread_ids[i]] != 0 && group->entries[i] != (void (*)(void))-1) {
                thread_start(group->thread_ids[i], group->entries[i]);
                thread_parameters_set(group->thread_ids[i], (s32)&g_menu_thread_menu_data[i], 0, 0);
                g_threads[group->thread_ids[i]].task_id = group->task_ids[i];
            }
        }
        if (g_threads[g_thread_current_id].function_parameter_1 == 0) {
            thread_current_exit();
        }
    }
}

/* Set the text image's upload rectangle and initialize its sprite. */
static void entry_text_sprite_init(RECT* out, const menu_entry_t* record, SPRT* primitive) {
    out->x = record->vram_origin.x;
    out->y = record->vram_origin.y;
    out->w = record->inner_width >> 2;
    out->h = record->inner_height;
    menu_init_sprt(primitive);
    primitive->clut = MENU_CLUT_NORMAL;
    gfx_set_image_primitive_layout(primitive, &record->vram_origin, &record->window, &record->text_image);
}

/* Set up the two menu sprites for one record. The first sprite is positioned
 * from the record's x/y according to its layout mode (1 or 2), the second is
 * a fixed 16x16 sprite. Both use CLUT 0x7d7c.
 *
 * `mode` is reused to carry the y coordinate once the mode test is done; that
 * reuse is what puts the y value in the target's v1. */
static void entry_frame_sprites_build(menu_entry_t* entry, SPRT sprite[2], s32 unused_2) {
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

/* Set which buttons the WORLD menus accept. */
void screen_allowed_buttons_set(u32 mask) {
    g_screen_allowed_buttons = mask;
}

/* Per-frame WORLD menu step: loads the input timing profile selected by
 * the finger cursor repeat speed option, derives the edge/auto-repeat 32-bit input mask
 * (masked by the allowed-button mask and cleared while any of the six
 * screen-request flags is set), advances the pending world-map screen
 * requests, publishes the input, yields once while tracking thread status
 * changes and returns the menu result slot g_command_pending_selection[0] (-2 while pending).
 * WORLD's sibling of menu_input_event_state_update. */
s16 screen_menu_frame_update(u32* otag, u32 input) {
    s32 profile = g_game_options.fields.finger_cursor_repeat_speed;
    u32 previous_input;
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_delay;
    u32 counter;
    s32 thread_id;
    s32 status;

    g_input_repeat_initial_delay = g_screen_input_timing_profiles[profile].initial_delay;
    g_input_repeat_period = g_screen_input_timing_profiles[profile].repeat_period;
    g_input_secondary_repeat_period = g_screen_input_timing_profiles[profile].secondary_repeat_period;
    g_menu_scroll_accel_delay = g_screen_input_timing_profiles[profile].scroll_accel_delay;
    g_menu_scroll_slow_step = g_screen_input_timing_profiles[profile].scroll_slow_step;
    g_menu_scroll_fast_step = g_screen_input_timing_profiles[profile].scroll_fast_step;
    if (input == 0) {
        g_screen_initial_repeat_counter = 0;
        g_screen_repeat_counter = 0;
        g_screen_secondary_repeat_counter = 0;
    }

    previous_input = g_screen_current_input;
    g_screen_current_input = input;
    g_screen_new_buttons = 0;
    g_screen_previous_input = previous_input;

    if (previous_input != input || (filtered_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_screen_new_buttons = ~previous_input & input;
        g_screen_initial_repeat_counter = 0;
    } else {
        initial_delay = g_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_screen_initial_repeat_counter;
        if (counter == initial_delay) {
            g_screen_new_buttons = filtered_input;
        }
        if (counter >= initial_delay) {
            repeat_delay = g_input_repeat_period / g_script_event_speed;
            counter = ++g_screen_repeat_counter;
            g_screen_secondary_repeat_counter++;
            if (counter >= repeat_delay) {
                g_screen_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_screen_new_buttons = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_screen_new_buttons = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_delay = g_input_secondary_repeat_period / g_script_event_speed;
            if (g_screen_secondary_repeat_counter >= secondary_delay) {
                g_screen_secondary_repeat_counter = 0;
                if ((g_screen_current_input & PSX_PAD_SQUARE) != 0) {
                    g_screen_new_buttons
                        |= g_screen_current_input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_screen_new_buttons |= g_screen_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_screen_repeat_counter = 0;
            g_screen_secondary_repeat_counter = 0;
        }
    }

    g_screen_new_buttons &= g_screen_allowed_buttons;
    if (g_formation_open_request_state != WORLD_MAP_REQUEST_IDLE) {
        g_screen_new_buttons = 0;
    }
    if (g_world_map_tutorial_request_state != WORLD_MAP_REQUEST_IDLE) {
        g_screen_new_buttons = 0;
    }
    if (g_world_map_debug_variable_list_request_state != 0) {
        g_screen_new_buttons = 0;
    }
    if (g_save_menu_request_state != WORLD_MAP_REQUEST_IDLE) {
        g_screen_new_buttons = 0;
    }
    if (g_world_map_brave_story_request_state != WORLD_MAP_REQUEST_IDLE) {
        g_screen_new_buttons = 0;
    }
    if (g_world_map_debug_party_number_request_state != 0) {
        g_screen_new_buttons = 0;
    }
    g_formation_screen_active = 1;
    formation_request_update();
    brave_story_request_update();
    save_request_update();
    tutorial_request_update();
    variable_list_request_update();
    party_number_request_update();
    g_command_pending_selection[0] = -2;
    g_gfx_current_otag_entry = otag;
    g_sound_pending_sfx_id = -1;
    menu_draw_active_window_frames();
    script_load_event_input_state(&g_screen_input_state);
    if (g_screen_new_buttons != PSX_PAD_SELECT && g_screen_new_buttons != 0) {
        g_script_input_suppression_frames = 5;
    }

    for (thread_id = 1; thread_id < THREAD_ID_COUNT; thread_id++) {
        g_menu_thread_snapshot[thread_id] = thread_is_running_asm(thread_id);
    }

    thread_yield();
    for (thread_id = 1; thread_id < THREAD_ID_COUNT; thread_id++) {
        status = g_menu_thread_snapshot[thread_id];
        if (status != thread_is_running_asm(thread_id)) {
            g_script_input_suppression_frames = 5;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }
    if (g_overlay_companion_executable_request_index != 0) {
        g_overlay_companion_executable_request_index = 0;
    }
    g_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_sound_pending_sfx_id != -1) {
        sound_free_channel_sfx_play(g_sound_pending_sfx_id);
    }
    if (g_command_pending_selection[0] == 0) {
        script_variable_set(EVENT_SCRIPT_VAR_CURRENT_MAP, g_command_pending_selection[1]);
        g_world_map_flags |= WORLD_MAP_FLAG_REQUEST_DESTINATION;
    }
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
    return g_command_pending_selection[0];
}

/* Opens menu entry `index` in THREAD_SLOT_MENU, first placing its window at the default
 * origin (-0x70, -0x50; the x and y arguments are ignored) and clamping that origin so the
 * window stays on screen. Does nothing while that slot runs. */
void screen_entry_window_open(s32 index, s32 unused_x, s32 unused_y) {
    menu_entry_t* entry;
    s32 x;
    s32 y;
    s32 initial_x = -0x70;
    s32 initial_y = -0x50;

    if (g_threads[THREAD_SLOT_MENU].is_running == 0) {
        entry = &g_screen_default_entries[index];
        g_menu_thread_menu_data = g_screen_default_entries;
        entry->window.x = initial_x;
        entry->window.y = initial_y;
        x = entry->window.x;
        g_screen_open_entry_index = index;
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
        thread_parameters_set(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[index], 0, 0);
    }
}

/* Build a 4bpp window-frame image from the shared menu graphics.
 *
 * Top and bottom bands are fixed; the middle 16 rows tile. The unused `s32`
 * return keeps $v0 live and preserves the target loop scheduling; the target
 * leaves the failed loop test's 0 there, or the partial 16-bit-element flag when `h` is
 * not positive.
 */
s32 screen_window_frame_image_build(s32 w, s32 h, u16* dst) {
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
        screen_build_window_image_row(src, row, words, src_index);
    }
}
