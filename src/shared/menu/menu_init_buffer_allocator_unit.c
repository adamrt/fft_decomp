#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Reset the menu buffer allocator: mark the 16 allocation records free (-1),
 * clear the page allocation map (VARIANT_MENU_BUFFER_PAGE_COUNT pages: 53 in
 * BATTLE, 112 in WORLD) and zero the three text slots' owner thread ids and
 * glyph counts. */
void menu_init_buffer_allocator(void) {
    s32 fill;
    s32 slot;
    s32 count;
    u8* byte;
    s32* first;
    s32* second;

    /* One counter local shared by the last two loops and each counter set
     * before its pointer: this reproduces the target's register assignment
     * without pins. */
    fill = -1;
    for (slot = MENU_BUFFER_ALLOCATION_COUNT - 1; slot >= 0; slot--) {
        g_menu_allocator_slot_records[slot].buffer = (u8*)fill;
    }
    count = VARIANT_MENU_BUFFER_PAGE_COUNT - 1;
    byte = &g_menu_buffer_allocation_map[VARIANT_MENU_BUFFER_PAGE_COUNT - 1];
    for (; count >= 0; count--) {
        *byte = 0;
        byte--;
    }
    count = 0;
    second = g_text_slot_glyph_counts;
    first = g_text_slot_owner_thread_ids;
    for (; count < 3; count++) {
        *first = 0;
        *second = 0;
        second++;
        first++;
    }
}

/* Claims enough 256-byte arena pages for size bytes and records them in a free slot.
 *
 * Scans the page occupancy map first-fit. When no run of free pages or no free slot
 * remains, it optionally prints a diagnostic, yields the thread, and rescans.
 * The arena holds VARIANT_MENU_BUFFER_PAGE_COUNT pages (53 in BATTLE, 112 in WORLD). */
void* menu_alloc_memory(s32 size) {
    s32 page;
    s32 pages;
    s32 i;
    u8* buffer;

    size = (size & 0xFF) != 0 ? (size >> 8) + 1 : size >> 8;
    for (;;) {
        for (page = 0; page < VARIANT_MENU_BUFFER_PAGE_COUNT; page++) {
            if (g_menu_buffer_allocation_map[page] == 0) {
                for (pages = 0;; pages++) {
                    if (pages + page >= VARIANT_MENU_BUFFER_PAGE_COUNT) {
                        /* Leaves the run and page loops for the retry wait. */
                        goto fail;
                    }
                    if (pages >= size || g_menu_buffer_allocation_map[pages + page] != 0) {
                        break;
                    }
                }
                if (pages >= size) {
                    for (pages = 0; pages < size; pages++) {
                        g_menu_buffer_allocation_map[page + pages] = 1;
                    }
                    for (i = 0; i < MENU_BUFFER_ALLOCATION_COUNT; i++) {
                        if (g_menu_allocator_slot_records[i].buffer == (u8*)-1) {
                            buffer = g_menu_buffer_arena[page];
                            g_menu_allocator_slot_records[i].buffer = buffer;
                            g_menu_allocator_slot_records[i].page_count = pages;
                            return buffer;
                        }
                    }
                    break;
                }
            }
        }
    fail:
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_text_wait_for_allocation_message, g_thread_current_id, size);
        }
        thread_yield();
    }
}

/* Thin public entry point for the BATTLE menu-buffer allocator. */
void* menu_alloc_buffer(s32 bytes) {
    return menu_alloc_memory(bytes);
}

/* Free a menu arena allocation; an unknown eligible pointer stops the current thread. */
void menu_free_memory(void* buffer_pointer) {
    u32 buffer = (u32)buffer_pointer;
    u32 grid_base;
    s32* allocation_size;
    menu_allocation_t* allocation;
    s32* allocation_end;
    u8* allocation_grid;
    s32 invalid_buffer;
    s32 page;

    if (buffer < (u32)g_main_heap_companion_overlay_load_address && buffer != -1) {
        invalid_buffer = -1;
        grid_base = (buffer - (u32)g_menu_buffer_arena) >> 8;
        allocation_grid = g_menu_buffer_allocation_map;
        /* A separate page-count cursor preserves the native count-field base. */
        allocation_size = &g_menu_allocator_slot_records[0].page_count;
        allocation = g_menu_allocator_slot_records;
        allocation_end = &g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT].page_count;
        for (;;) {
            if ((u32)allocation->buffer == buffer) {
                allocation->buffer = (u8*)invalid_buffer;
                /* Written as (index + base) + grid so the two addu operands
                 * come out in the target's order; the size is reloaded
                 * each iteration because the byte store may alias it. */
                for (page = 0; page < *allocation_size; page++) {
                    *(u8*)((page + grid_base) + (u32)allocation_grid) = 0;
                }
                return;
            }
            allocation_size += sizeof(menu_allocation_t) / sizeof(*allocation_size);
            allocation++;
            if ((s32)allocation_size >= (s32)allocation_end) {
                thread_exit_current();
                return;
            }
        }
    }
}

/* Build the twelve line packets, draw modes and covering frame sprite. */
void menu_init_numeric_display_frame_primitives(RECT* box, menu_palette_primitives_t* menu) {
    LINE_F2* line;
    s32 i;
    s32 offset;

    line = menu->lines;
    for (i = 0; i < 12; i++) {
        SetLineF2(line);
        line++;
    }
    menu_init_primitive_colors_normal(menu);
    line = menu->lines;
    offset = 0;
    for (i = 0; i < 12; i++) {
        line->x0 = g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_X0] + box->x;
        line->y0 = g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_Y0] + box->y;
        line->x1 = g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_X1] + box->x;
        line->y1 = g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_Y1] + box->y;
        if (g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_W0] != 0) {
            line->x0 += box->w;
        }
        if (g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_H0] != 0) {
            line->y0 += box->h;
        }
        if (g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_W1] != 0) {
            line->x1 += box->w;
        }
        if (g_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_H1] != 0) {
            line->y1 += box->h;
        }
        offset += WORLD_MENU_LINE_LAYOUT_SIZE;
        line++;
    }
    SetDrawMode(&menu->draw_mode_menu, 1, 0, GetTPage(0, 0, 0x3c0, 0x100), &g_menu_numeric_display_texture_window);
    SetDrawMode(&menu->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
    menu_init_sprt_variant(&menu->sprite);
    menu->sprite.x0 = box->x;
    menu->sprite.y0 = box->y;
    menu->sprite.w = box->w;
    menu->sprite.h = box->h;
}

/* Submit the frame's twelve lines, sprite and draw modes. */
void menu_submit_numeric_display_frame_primitives(menu_palette_primitives_t* display) {
    s32 index;

    for (index = 0; index < 12; index++) {
        gfx_draw_or_append_gpu_primitive(&display->lines[index]);
    }
    gfx_draw_or_append_gpu_primitive(&display->draw_mode);
    gfx_draw_or_append_gpu_primitive(&display->sprite);
    gfx_draw_or_append_gpu_primitive(&display->draw_mode_menu);
}

/* Give the twelve frame lines the colors of the normal menu palette (the first
 * sixteen of the menu palette colors) and the sprite the normal menu CLUT, GetClut(0x3c0, 0x1f0). */
void menu_init_primitive_colors_normal(menu_palette_primitives_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    for (index = 0, line = menu->lines; index < 12; index++, line++) {
        palette_index = g_menu_numeric_display_palette_indices[index];
        line->r0 = g_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_menu_palette_colors.colors[palette_index].blue << 3;
    }
    menu->sprite.clut = 0x7c3c;
}

/* Give the twelve frame lines the colors of the dimmed menu palette (the second
 * sixteen of the menu palette colors) and the sprite the dimmed menu CLUT, GetClut(0x3c0, 0x1f4). */
void menu_init_primitive_colors_dimmed(menu_palette_primitives_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    for (index = 0, line = menu->lines; index < 12; index++, line++) {
        palette_index = (g_menu_numeric_display_palette_indices[index] + PALETTE_COLOR_COUNT);
        line->r0 = g_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_menu_palette_colors.colors[palette_index].blue << 3;
    }
    menu->sprite.clut = 0x7d3c;
}
