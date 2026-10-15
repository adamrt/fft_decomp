#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"

/* Uploads the 20 party portraits (sprite and palette cells) from the 0x18ba
 * portrait sheet file to VRAM.
 *
 * Register allocation and scheduling depend on the variable shapes: `value`
 * doubles as the pixel-row counter, `row` also holds the palette remainder,
 * the job arms reread `unit->job_id`, `offset` is built in three statements
 * (multi-set temps keep the i / 7 mult after the row * 0x3000 terms), and the
 * palette offset has its own variable. */
void require_gfx_prepare_party_portrait_textures(void) {
    menu_portrait_sheet_t* file;
    s32 offset;
    s32 pal_offset;
    s32 i;
    s32 value;
    party_unit_t* unit;
    s32 job_id;
    s32 base;
    s32 cell;
    s32 palette;
    s32 sheet;
    s32 row;
    s32 col;
    u8* src;
    RECT unused[2];

    file = main_heap_alloc(MENU_WLDFACE_FILE_SIZE);
    if (g_script_attack_entry_mode != 0) {
        g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_WLDFACE_BIN, MENU_WLDFACE_FILE_SIZE, file);
    } else {
        StoreImage(&g_require_portrait_sheet_rect_0, (u32*)file[0].pixels);
        StoreImage(&g_require_portrait_sheet_rect_1, (u32*)file[1].pixels);
        StoreImage(&g_require_portrait_sheet_rect_2, (u32*)file[2].pixels);
        StoreImage(&g_require_portrait_sheet_rect_3, (u32*)file[3].pixels);
        DrawSync(0);
    }
    for (i = 0; i < 0x15; i++) {
        if (i == 20) {
            break;
        }
        unit = main_party_get_unit(i);
        if (unit->sprite_set < CHARACTER_IDENTITY_SELECTOR_FIRST) {
            value = unit->sprite_set;
        } else {
            if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
                base = (unit->job_id - JOB_ID_GENERIC_FIRST) * 2;
                value = base + 0x60;
                if (unit->job_id == JOB_ID_MIME) {
                    value = base + 0x5e;
                }
            } else if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
                base = (unit->job_id - JOB_ID_GENERIC_FIRST) * 2;
                value = base + 0x61;
                if (unit->job_id == JOB_ID_MIME) {
                    value = base + 0x5f;
                }
            } else {
                g_require_formation_job_data = main_job_get_data_pointer(unit->job_id);
                value = g_require_formation_job_data->spritesheet_id;
            }
            job_id = unit->job_id;
            if (job_id == JOB_ID_BARD) {
                value = 0x82;
            }
            if (job_id == JOB_ID_DANCER) {
                value = 0x83;
            }
        }
        value = g_require_portrait_index_by_sprite_set[value];
        if (unit->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
            value = 0;
        }
        if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
            value |= g_require_formation_job_data->job_portrait_palette << 10;
        }
        cell = value & 0xff;
        palette = (value & 0xc00) >> 10;
        sheet = cell / MENU_WLDFACE_PORTRAITS_PER_SHEET;
        value = cell - sheet * MENU_WLDFACE_PORTRAITS_PER_SHEET;
        row = value / 8;
        col = value % 8;
        g_require_portrait_cell_rect.x = (i % 7) * 8 + 0x100;
        g_require_portrait_cell_rect.y = (i / 7) * 0x30 + 0x28;
        g_require_portrait_cell_rect.w = 8;
        g_require_portrait_cell_rect.h = 0x30;
        value = 0;
        offset = row * 2;
        offset += row;
        offset <<= 12;
        offset += col * 0x20;
        offset >>= 1;
        /* Ordinary sheet indexing reverses the native ADDU operands. */
        src = (u8*)(sheet * MENU_WLDFACE_SHEET_SIZE + (s32)file + offset);
        for (; value < 0x30; value++) {
            for (col = 0; col < 0x10; col++) {
                g_require_portrait_cell_pixels[value * 0x10 + col] = src[col];
            }
            src += 0x80;
        }
        LoadImage(i == 20 ? &g_require_special_portrait_rect : &g_require_portrait_cell_rect,
            (u32*)g_require_portrait_cell_pixels);
        g_require_portrait_palette_rect.x = (i % 3) * 0x10 + 0x100;
        g_require_portrait_palette_rect.y = i / 3;
        g_require_portrait_palette_rect.w = 0x10;
        g_require_portrait_palette_rect.h = 1;
        sheet = cell / MENU_WLDFACE_PORTRAITS_PER_SHEET;
        value = cell - sheet * MENU_WLDFACE_PORTRAITS_PER_SHEET;
        row = value / 4;
        col = value % 4;
        value = row;
        if (palette != 0) {
            sheet = (cell - MENU_WLDFACE_FIRST_MONSTER_PORTRAIT) / MENU_WLDFACE_MONSTER_PALETTE_ROW_COUNT;
            col = palette;
            row = (cell - MENU_WLDFACE_FIRST_MONSTER_PORTRAIT) % MENU_WLDFACE_MONSTER_PALETTE_ROW_COUNT;
            value = row + MENU_WLDFACE_MONSTER_PALETTE_ROW;
        }
        pal_offset = value * 0x100 + 0xf000 + col * 0x40 >> 1;
        /* Palette member indexing changes the native offset calculation and allocation. */
        if (i == 20) {
            LoadImage(&g_require_special_portrait_palette_rect, (u32*)((u8*)&file[sheet] + pal_offset));
        } else {
            LoadImage(&g_require_portrait_palette_rect, (u32*)((u8*)&file[sheet] + pal_offset));
        }
        DrawSync(0);
    }
    main_heap_call_free(file);
}
