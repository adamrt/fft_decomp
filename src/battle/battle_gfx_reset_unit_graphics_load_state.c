#include "fft/battle.h"
#include "fft/battle_gfx.h"

/* The two clearing loops walk their arrays by byte offset. A struct-subscript
 * walk leaves GCC with both the index and the scaled address as induction
 * variables, adding an `addiu`/`li` pair to each loop; the byte offset is what
 * reproduces the target's single `bgez` on the address offset. */
void battle_gfx_reset_unit_graphics_load_state(void) {
    s32 i;
    s32 offset;

    g_battle_misc_unit_list_head = 0;
    for (offset = 15 * 0x440; offset >= 0; offset -= 0x440) {
        *(s32*)&((u8*)g_battle_unit_misc_slot_flags)[offset] = 0;
    }
    for (offset = 8 * 0x32d6; offset >= 0; offset -= 0x32d6) {
        ((u8*)g_battle_gfx_spritesheet_slots)[offset] = 0;
    }
    for (i = 0; i < 2; i++) {
        g_battle_gfx_vram_slots[i].owner = 0xff;
        g_battle_gfx_vram_slots[i].evtchr_load_marker = 0;
        g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
    }
    g_battle_gfx_vram_slots[0].image_rect.x = 0x100;
    g_battle_gfx_vram_slots[0].image_rect.y = 0;
    g_battle_gfx_vram_slots[0].image_rect.w = 0x40;
    g_battle_gfx_vram_slots[0].image_rect.h = 0xc8;
    g_battle_gfx_vram_slots[1].image_rect.x = 0x140;
    g_battle_gfx_vram_slots[1].image_rect.y = 0;
    g_battle_gfx_vram_slots[1].image_rect.w = 0x40;
    g_battle_gfx_vram_slots[1].image_rect.h = 0xc8;
    g_battle_gfx_load_data_cursor = (u8*)&g_battle_gfx_unit_animation_data;
    g_battle_gfx_state_words[0] = 0;
    g_battle_gfx_state_words[1] = 0;
    g_battle_gfx_state_words[2] = 0;
    g_battle_gfx_state_words[3] = 0;
    g_battle_gfx_shp_frame_data_cursor = (u8*)&g_battle_gfx_shp_frame_data_buffer;
}
