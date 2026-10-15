/* WLDFACE, common graphics and menu text loading. */

#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Open WLDFACE.BIN and move it to the frame buffer.
 *
 * The file at disc sector 0x18ba is fetched on the main thread and moved to
 * VRAM as its four sheets, each with its portraits' CLUTs below them.
 */
void world_gfx_load_wldface_to_frame_buffer(void) {
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
void world_menu_load_common_graphics(s32 load_extra_pages) {
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

void world_menu_set_default_entry_1_text_values(s32 value) {
    g_world_menu_default_entry_1_text_values[0] = value;
    g_world_menu_default_entry_1_text_values[1] = 0;
}

/* Variant of world_menu_load_text_1bd8 that loads no graphics and reads the
 * 0x5800-byte text file at sector 0x1C18; all 0x20 section pointers come
 * from the file, relative to the end of its offset table. */
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
 * the 0xe800-byte menu text file at sector 0x1bd8 and the default menu
 * entries.
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
