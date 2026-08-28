#include "fft/attack.h"
#include "fft/data.h"
#include "fft/main_file.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/thread.h"
#include "psx/gpu.h"

/* This overlay reads BATTLE's attack entry mode unsigned (lhu). */
#define ENTRY_MODE (*(u16*)&g_battle_script_attack_entry_mode)

/* Uploads the party portrait sprite and palette cells from the 0x18ba portrait
 * sheet file to VRAM; slot 20 uses the portrait id in g_main_special_portrait_wldface_id.
 *
 * Port of require_prepare_party_portrait_textures. Here `value` is the party
 * index passed to main_party_get_data_pointer, so the pixel-row counter is a
 * separate variable `j`. The never-taken `j <= 0` test after the row loop only
 * adds one reference to `j`, raising its global-allocation priority above the
 * source pointer's so the counter gets $a3 as in the target. */
void attack_prepare_party_portrait_textures(void) {
    s32 file;
    s32 offset;
    s32 pal_offset;
    s32 i;
    s32 value;
    party_data_t* unit;
    s32 job_id;
    s32 base;
    s32 cell;
    s32 palette;
    s32 sheet;
    s32 row;
    s32 col;
    s32 j;
    u8* src;
    RECT unused[2];

    file = (s32)main_heap_alloc(0x20000);
    if (ENTRY_MODE != 0) {
        if (ENTRY_MODE == 2) {
            attack_file_load_sync(0x18ba, 0x20000, file);
        } else {
            g_battle_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
            battle_thread_call_on_main_stack(0x18ba, 0x20000, file);
        }
    } else {
        StoreImage(&g_attack_portrait_sheet_rect_0, (u32*)file);
        StoreImage(&g_attack_portrait_sheet_rect_1, (u32*)(file + 0x8000));
        StoreImage(&g_attack_portrait_sheet_rect_2, (u32*)(file + 0x10000));
        StoreImage(&g_attack_portrait_sheet_rect_3, (u32*)(file + 0x18000));
        DrawSync(0);
    }
    for (i = 0; i < 0x15; i++) {
        if (i == 20) {
            value = g_main_special_portrait_wldface_id;
        } else {
            value = i;
        }
        if (i != 20) {
            unit = main_party_get_data_pointer(value);
            if (unit->sprite_set < 0x80) {
                value = unit->sprite_set;
            } else {
                if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
                    base = (unit->job_id - 0x4a) * 2;
                    value = base + 0x60;
                    if (unit->job_id == JOB_ID_MIME) {
                        value = base + 0x5e;
                    }
                } else if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
                    base = (unit->job_id - 0x4a) * 2;
                    value = base + 0x61;
                    if (unit->job_id == JOB_ID_MIME) {
                        value = base + 0x5f;
                    }
                } else {
                    g_attack_formation_job_data = main_job_get_data_pointer(unit->job_id);
                    value = g_attack_formation_job_data->spritesheet_id;
                }
                job_id = unit->job_id;
                if (job_id == JOB_ID_BARD) {
                    value = 0x82;
                }
                if (job_id == JOB_ID_DANCER) {
                    value = 0x83;
                }
            }
            value = g_attack_portrait_index_by_sprite_set[value];
            if (unit->sprite_set == 0) {
                value = 0;
            }
            if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
                value |= g_attack_formation_job_data->job_portrait_palette << 10;
            }
        }
        cell = value & 0xff;
        palette = (value & 0xc00) >> 10;
        sheet = cell / 40;
        value = cell - sheet * 40;
        row = value / 8;
        col = value % 8;
        g_attack_portrait_upload_rect.x = (i % 7) * 8 + 0x100;
        g_attack_portrait_upload_rect.y = (i / 7) * 0x30 + 0x28;
        g_attack_portrait_upload_rect.w = 8;
        g_attack_portrait_upload_rect.h = 0x30;
        j = 0;
        offset = row * 2;
        offset += row;
        offset <<= 12;
        offset += col * 0x20;
        offset >>= 1;
        src = (u8*)(sheet * 0x8000 + file + offset);
        for (; j < 0x30; j++) {
            for (col = 0; col < 0x10; col++) {
                g_attack_portrait_staging[j * 0x10 + col] = src[col];
            }
            src += 0x80;
        }
        if (j <= 0) {
            j = 0;
        }
        LoadImage(i == 20 ? &g_attack_special_portrait_rect : &g_attack_portrait_upload_rect,
            (u32*)g_attack_portrait_staging);
        g_attack_portrait_palette_upload_rect.x = (i % 3) * 0x10 + 0x100;
        g_attack_portrait_palette_upload_rect.y = i / 3;
        g_attack_portrait_palette_upload_rect.w = 0x10;
        g_attack_portrait_palette_upload_rect.h = 1;
        sheet = cell / 40;
        value = cell - sheet * 40;
        row = value / 4;
        col = value % 4;
        value = row;
        if (palette != 0) {
            sheet = (cell - 0x3f) / 6;
            col = palette;
            row = (cell - 0x3f) % 6;
            value = row + 10;
        }
        pal_offset = value * 0x100 + 0xf000 + col * 0x40 >> 1;
        if (i == 20) {
            LoadImage(&g_attack_special_portrait_palette_rect, (u32*)(file + sheet * 0x8000 + pal_offset));
        } else {
            LoadImage(&g_attack_portrait_palette_upload_rect, (u32*)(file + sheet * 0x8000 + pal_offset));
        }
        if (ENTRY_MODE == 2) {
            battle_thread_yield();
        } else {
            DrawSync(0);
        }
    }
    main_heap_call_free((void*)file);
}
