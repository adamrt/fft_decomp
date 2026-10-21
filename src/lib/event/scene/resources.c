/* Load event scripts, portraits and EVTCHR images and initialize menu text and palettes. */

#include "fft/file.h"
#include "fft/fullscreen_graphic.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/overlay.h"
#include "fft/portrait.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/sprite_sheet.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* One EVTFACE.BIN block: pixels followed by eight 16-color palettes. */
typedef struct portrait_image_block {
    u32 pixels[8][48][4];  /* 0x0000; eight 32x48 4bpp portraits */
    palette_t palettes[8]; /* 0x1800; four palettes per uploaded VRAM row */
    u8 _unused_1900[0x700];
} portrait_image_block_t;
typedef char portrait_image_block_size_must_be_8192[sizeof(portrait_image_block_t) == 8192 ? 1 : -1];

/* The two programs reserve different menu image columns for EVTFACE. */
#ifdef FFT_BATTLE
static RECT s_menu_portrait_pixel_rect = { 448, 206, 64, 48 };

static RECT s_menu_portrait_palette_rect = { 448, 254, 64, 2 };

#else
static RECT s_menu_portrait_pixel_rect = { 320, 206, 64, 48 };

static RECT s_menu_portrait_palette_rect = { 320, 254, 64, 2 };

#endif

/* Thread body of the ShowGraphic event instruction: load the ETC overlay and show its graphic. */
void script_graphic_show(void) {
    thread_current_task_set(THREAD_TASK_SHOW_GRAPHIC);
    overlay_companion_request(COMPANION_OVERLAY_ETC);
    etc_entrypoint();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_graphic_show and
 * portrait_image_load. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_3(void) {
}

/* Upload one EVTFACE.BIN block as a single image, followed by its palette rows. */
void portrait_image_load(s32 portrait_id) {
    portrait_image_block_t* buffer;

    buffer = menu_alloc_memory(sizeof(*buffer));
    g_thread_call_target = (void (*)(void))file_buffer_load_checked;
    thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer);
    LoadImage(&s_menu_portrait_pixel_rect, buffer->pixels[0][0]);
    LoadImage(&s_menu_portrait_palette_rect, (u32*)buffer->palettes);
    menu_free_memory(buffer);
}

/*
 * Loads one 0x7800-byte block of EVTCHR.BIN into a fresh heap allocation, with the read
 * and its polling run on the main stack. Parameter 1 is the EVTCHR slot and parameter 2
 * the block; each block occupies 15 sectors from 0x1d4c. The block is published through
 * g_script_pending_loaded_evtchr_slot and g_script_loaded_evtchr_buffer for one frame, during
 * which BATTLE's per-frame event update unpacks it into the slot, and then freed.
 * WORLD retains this loader, but its per-frame request hook is empty.
 */
void gfx_load_evtchr_thread(void) {
    s32 slot = thread_current_parameter_1_get();
    s32 block = thread_current_parameter_2_get();
    evtchr_file_record_t* buffer;

    thread_current_task_set(THREAD_TASK_LOAD_EVTCHR);
    buffer = heap_alloc(0x7800);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))file_bytes_read_request;
    } while (thread_call_on_main_stack((block * 15) + DISC_SECTOR_EVENT_EVTCHR_BIN, 0x7800, buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))file_load_state_get;
    } while (thread_call_on_main_stack() != 0);
    g_script_pending_loaded_evtchr_slot = slot;
    g_script_loaded_evtchr_buffer = buffer;
    thread_yield();
    heap_free(buffer);
    thread_current_exit();
}

/* Uploads eight 32x48 portraits and their palettes from one EVTFACE.BIN block, yielding while
 * the file loads. Each portrait is 8 VRAM columns (32 4-bit pixels) wide, placed side by side
 * from g_menu_portrait_pixel_rect. */
void script_opcode_50_portrait_col(s32 portrait_id) {
    RECT rect;
    portrait_image_block_t* buffer;
    u32(*pixels)[48][4];
    s32 i;
    palette_t* palette;

    buffer = menu_alloc_memory(sizeof(*buffer));
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))file_bytes_read_request;
    } while (thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = file_load_state_get;
    } while (thread_call_on_main_stack() != 0);

    palette = buffer->palettes;
    mem_copy_bytes_nonempty(&rect, &s_menu_portrait_pixel_rect, sizeof(rect));
    i = 0;
    pixels = buffer->pixels;
    rect.w = 8;
    rect.h = 0x30;
    do {
        LoadImage(&rect, pixels[0][0]);
        rect.x += 8;
        pixels += 1;
        i += 1;
    } while (i < 8);
    LoadImage(&s_menu_portrait_palette_rect, (u32*)palette);
    menu_free_memory(buffer);
    thread_yield();
}

/* Read event block `id` (EVENT_BLOCK_SECTORS sectors from the event archive) into
 * the script event block, yielding until the load finishes, then turn the block's
 * text offset into the event-message section pointer.
 *
 * The 32-bit offset is then overwritten with EVENT_TEXT_OFFSET_PROCESSED: the
 * interpreter starts at byte zero of the block and reads those four bytes as
 * one-byte NOP opcodes. A block whose first 32-bit value already holds that marker leaves
 * the section pointer alone. Both the read request and the load poll run on the
 * main stack. */
void script_event_load(s32 id) {
    event_file_block_t* block;
    u32 off;

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))file_bytes_read_request;
    } while (thread_call_on_main_stack(
                 id * EVENT_BLOCK_SECTORS + DISC_SECTOR_EVENT_TEST_EVT, EVENT_BLOCK_BYTES, &g_script_event_block_buffer)
        != 0);

    do {
        thread_yield();
        g_thread_call_target = file_load_state_get;
    } while (thread_call_on_main_stack() != 0);

    block = g_script_event_block;
    off = block->text_offset_or_marker;
    if (off != EVENT_TEXT_OFFSET_PROCESSED) {
        /* Integer addition retains the native addu operand order. */
        g_text_section_pointers[TEXT_SECTION_EVENT_MESSAGES] = (u8*)(off + (u32)block);
        block->text_offset_or_marker = EVENT_TEXT_OFFSET_PROCESSED;
    }
}

/* Copy a normal and a dimmed menu palette (16 colors each) into g_menu_palette_colors, which the
 * panel frame lines take their colors from. */
void menu_palette_colors_set(const menu_palette_pair_t* source) {
    rgba5551_t* destination = g_menu_palette_colors.colors;

    mem_copy_bytes_nonempty(destination, source->colors, sizeof(palette_t));
    mem_copy_bytes_nonempty(destination + PALETTE_COLOR_COUNT, source->colors + PALETTE_COLOR_COUNT, sizeof(palette_t));
}

/* Load the default menu palettes and rebuild the text section pointers. */
void menu_palette_text_init(s32 unused_0) {
    menu_palette_colors_set(&g_menu_glyph_image_data.palette);
    text_init_section_pointers();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_palette_text_init and overlay_companion_file_poll. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void menu_noop_4(void) {
}
