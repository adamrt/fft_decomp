/* Loads file 0x166b (0x10000 bytes) via the entry-mode load paths, uploads its
 * second half, then for the 16 formation slots derives each unit's portrait
 * (inlined world_unit_get_portrait_index logic), copies a 0x18x0x30 cell into
 * the g_attack_formation_sprite_staging staging buffer, uploads it to g_attack_portrait_upload_rect and uploads the
 * 16x1 palette row to g_attack_portrait_palette_upload_rect; g_attack_formation_sprite_tall_cell[slot] records the cell
 * layout.
 *
 * Load-bearing shapes: the nested else routes the remainder<8 and (code-8)/5
 * arms through `product` and a trailing `row = product` copy, so jump2 sees
 * two different join labels and keeps the two identical `code / 10` arms instead
 * of cross-jumping them. `palette_base` (a multi-block variable) holds the
 * source offset sum so it is not tied to the dying `>> 1` temporary, and
 * `y` is initialised between that sum and `source`. `code` is one variable
 * reused for sheet, code, remainder and the final portrait code; the clear
 * loop counter reuses `x`; `source` is an integer so `x + source` keeps its
 * order. The palette offset's `/ 2` of an even sum compiles to the target's
 * plain `sra` and its allocation order; it also adds an 8-byte compiler
 * stack slot, so `unused` covers only the remaining 16 bytes. */
#include "fft/attack.h"
#include "fft/character_identity.h"
#include "fft/data.h"
#include "fft/job.h"
#include "fft/main_file.h"
#include "fft/main_heap.h"
#include "fft/main_unit.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* This overlay reads BATTLE's attack entry mode unsigned (lhu). */
#define ENTRY_MODE (*(u16*)&g_battle_script_attack_entry_mode)

void attack_gfx_prepare_formation_sprites(void) {
    u8 unused[0x10];
    u8* buffer;
    s32 i;
    party_data_t* party;
    s32 base;
    s32 code;
    u8* pixel;
    s32 portrait;
    s32 quotient;
    s32 row;
    s32 cell_row;
    s32 column;
    s32 product;
    s32 offset;
    s32 palette_base;
    s32 x;
    s32 y;
    s32 source;

    buffer = main_heap_alloc(0x10000);
    if (ENTRY_MODE != 0) {
        if (ENTRY_MODE == 2) {
            attack_file_load_sync(0x166B, 0x10000, buffer);
        } else {
            g_battle_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
            battle_thread_call_on_main_stack(0x166B, 0x10000, buffer);
        }
    } else {
        StoreImage(&g_attack_formation_sheet_rect, (u32*)buffer);
        StoreImage(&g_attack_formation_sheet_source_rect, (u32*)(buffer + 0x8000));
        DrawSync(0);
    }
    LoadImage(&g_attack_formation_sheet_rect, (u32*)(buffer + 0x8000));
    if (ENTRY_MODE == 2) {
        battle_thread_yield();
    } else {
        DrawSync(0);
    }
    for (i = 0; i < 0x10; i++) {
        party = main_party_get_data_pointer(i);
        if (party->sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
            if (party->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
                base = (party->job_id - JOB_ID_SQUIRE) * 2;
                code = base + 0x60;
                if (party->job_id == JOB_ID_MIME) {
                    code = base + 0x5E;
                }
            } else if (party->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
                base = (party->job_id - JOB_ID_SQUIRE) * 2;
                code = base + 0x61;
                if (party->job_id == JOB_ID_MIME) {
                    code = base + 0x5F;
                }
            } else {
                g_attack_formation_job_data = main_job_get_data_pointer(party->job_id);
                code = g_attack_formation_job_data->spritesheet_id;
            }
            if (party->job_id == JOB_ID_BARD) {
                code = 0x82;
            }
            if (party->job_id == JOB_ID_DANCER) {
                code = 0x83;
            }
        } else {
            code = party->sprite_set;
        }
        code = g_attack_portrait_index_by_sprite_set[code];
        if (party->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
            code = 0;
        }
        if (party->sprite_set == CHARACTER_IDENTITY_MONSTER) {
            code |= g_attack_formation_job_data->job_portrait_palette << 10;
        }
        code &= 0xFF;
        if (code == 0x18) {
            portrait = -1;
            code = 0x4C;
        } else if (code >= 0x3F) {
            code = g_attack_monster_formation_sprite_table[code];
            portrait = code;
        } else {
            portrait = code;
            if (party->sprite_set & CHARACTER_IDENTITY_HIGH_BIT_MASK) {
                code--;
                portrait = code;
            }
        }
        quotient = code / 60;
        code %= 60;
        if (quotient == 0) {
            row = code / 10;
            column = code % 10;
        } else {
            if (code < 8) {
                product = code / 10;
                column = code % 10;
            } else {
                product = (code - 8) / 5;
                column = (code - 8) % 5;
            }
            row = product;
        }
        cell_row = row;
        x = 0x47F;
        pixel = &g_attack_formation_sprite_staging[0x47F];
        do {
            *pixel = 0;
            x--;
            pixel--;
        } while (x >= 0);
        if (quotient == 1 && code >= 8) {
            palette_base = (quotient << 15) + ((column * 0x30 + cell_row * 0x3000 + 0x2800) >> 1);
            y = 0;
            source = (s32)buffer + palette_base;
            g_attack_portrait_upload_rect.x = (i % 5) * 0xC + 0x180;
            g_attack_portrait_upload_rect.y = (i / 5) * 0x30;
            g_attack_portrait_upload_rect.w = 0xC;
            g_attack_portrait_upload_rect.h = 0x30;
            for (; y < 0x30; y++) {
                for (x = 0; x < 0x18; x++) {
                    g_attack_formation_sprite_staging[y * 0x18 + x] = *(u8*)(y * 0x80 + (source + x));
                }
            }
            LoadImage(&g_attack_portrait_upload_rect, (u32*)g_attack_formation_sprite_staging);
            g_attack_formation_sprite_tall_cell[i] = 1;
        } else {
            palette_base = (quotient << 15) + ((column * 0x18 + cell_row * 0x2800) >> 1);
            y = 8;
            source = (s32)buffer + palette_base;
            g_attack_portrait_upload_rect.x = (i % 5) * 0xC + 0x180;
            g_attack_portrait_upload_rect.y = (i / 5) * 0x30;
            g_attack_portrait_upload_rect.w = 0xC;
            g_attack_portrait_upload_rect.h = 0x30;
            for (; y < 0x30; y++) {
                for (x = 6; x < 0x12; x++) {
                    g_attack_formation_sprite_staging[y * 0x18 + x] = *(u8*)(x + source + ((y - 8) * 0x80 - 6));
                }
            }
            LoadImage(&g_attack_portrait_upload_rect, (u32*)g_attack_formation_sprite_staging);
            g_attack_formation_sprite_tall_cell[i] = 0;
        }
        code = portrait;
        if (code == -1) {
            code = 0x7A;
        } else if (code >= 0x3E) {
            product = (code - 0x3E) * 3;
            palette_base = g_attack_formation_job_data->job_portrait_palette + 0x3E;
            code = product + palette_base;
        }
        row = code / 4;
        column = code % 4;
        offset = (row * 0x100 + column * 0x40) / 2 + 0xF000;
        g_attack_portrait_palette_upload_rect.x = (i % 4) * 0x10 + 0x180;
        g_attack_portrait_palette_upload_rect.y = i / 4 + 0xE0;
        g_attack_portrait_palette_upload_rect.w = 0x10;
        g_attack_portrait_palette_upload_rect.h = 1;
        LoadImage(&g_attack_portrait_palette_upload_rect, (u32*)(buffer + offset));
        if (ENTRY_MODE == 2) {
            battle_thread_yield();
        } else {
            DrawSync(0);
        }
    }
    main_heap_call_free(buffer);
}
